--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------


--------------------------------------------------------------------------------
-- AXI Write-Channel Datapath                                axi_write_datapath
--
-- Owns all write-side registers and arithmetic for an AXI4 slave.
-- Works in conjunction with an external write FSM (axi_write_fsm /
-- fb_write_fsm) which supplies the one-hot state inputs and receives the
-- handshake and beat-qualifier outputs needed to drive its next-state logic.
--
-- Responsibilities
-- ----------------
--   - Latch AW-channel sideband fields (ID, LEN, BURST, error flag) on the
--     AW handshake.
--   - Maintain the write address register: load on AW handshake, increment
--     by DATA_WIDTH/8 on each full-word non-last beat.
--   - Maintain the beat counter: load from AWLEN on AW handshake, decrement
--     on each full-word non-last beat.
--   - Latch WDATA and WSTRB on every W handshake in W_DATA.
--   - Compute the RMW byte-merge: per-byte mux of wdata_q vs rmw_rdata
--     controlled by wstrb_q.
--   - Drive the AXI write-channel outputs (AWREADY, WREADY, BVALID, BID,
--     BRESP) directly from FSM state and latched fields.
--   - Drive the memory-bus write port: mem_waddr, mem_wdata, mem_wstrb,
--     mem_we, mem_rmw_re, mem_rmw_we.
--   - Expose waddr_q, wdata_q, wstrb_q, rmw_merged, w_full_beat,
--     w_full_beat_cont as outputs for use by the parent
--     (BRAM arbiter, address decode, etc.).
--
-- Handshake outputs fed back to the FSM
-- --------------------------------------
--   aw_hs      AW handshake pulse  (awvalid AND awready, i.e. fsm_idle)
--   w_hs       W  handshake pulse  (wvalid  AND wready,  i.e. fsm_data)
--   b_hs       B  handshake pulse  (bvalid  AND bready)
--   w_partial  '1' when live WSTRB not all-1 on a single-beat transfer
--   w_last     '1' when the beat counter has reached zero
--
-- Port naming matches the AXI4 spec with an s_axi_ prefix for slave-side
-- signals.  The rst input is active-high (derived from aresetn inversion in
-- the parent).
--
-- Generics
-- --------
--   ADDR_WIDTH   AXI byte-address width        (default 20)
--   DATA_WIDTH   AXI data bus width in bits     (default 32)
--   ID_WIDTH     AXI ID field width             (default  4)
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity axi_write_datapath is
  generic (
    ADDR_WIDTH : integer := 32;
    DATA_WIDTH : integer := 32;
    ID_WIDTH   : integer := 4
  );
  port (
    clk  : in std_logic;
    rst  : in std_logic;

    ---------------------------------------------------------------------------
    -- AXI4 Write Address Channel (slave)
    ---------------------------------------------------------------------------
    awid    : in  std_logic_vector(ID_WIDTH-1   downto 0);
    awaddr  : in  std_logic_vector(ADDR_WIDTH-1 downto 0);
    awlen   : in  std_logic_vector(7  downto 0);
    awburst : in  std_logic_vector(1  downto 0);
    awvalid : in  std_logic;
    awready : out std_logic;   -- driven by fsm_idle

    ---------------------------------------------------------------------------
    -- AXI4 Write Data Channel (slave)
    ---------------------------------------------------------------------------
    wdata  : in  std_logic_vector(DATA_WIDTH-1   downto 0);
    wstrb  : in  std_logic_vector(DATA_WIDTH/8-1 downto 0);
    wvalid : in  std_logic;
    wready : out std_logic;    -- driven by fsm_data

    ---------------------------------------------------------------------------
    -- AXI4 Write Response Channel (slave)
    ---------------------------------------------------------------------------
    bid    : out std_logic_vector(ID_WIDTH-1 downto 0);
    bresp  : out std_logic_vector(1 downto 0);
    bvalid : out std_logic;    -- driven by fsm_resp
    bready : in  std_logic;

    ---------------------------------------------------------------------------
    -- FSM one-hot state inputs
    ---------------------------------------------------------------------------
    fsm_idle  : in std_logic;  -- W_IDLE : AWREADY asserted
    fsm_data  : in std_logic;  -- W_DATA : WREADY  asserted
    fsm_rmw_r : in std_logic;  -- W_RMW_R: RMW read  cycle
    fsm_rmw_w : in std_logic;  -- W_RMW_W: RMW write cycle
    fsm_resp  : in std_logic;  -- W_RESP : BVALID  asserted

    ---------------------------------------------------------------------------
    -- Handshake / qualifier outputs (fed back to FSM next-state logic)
    ---------------------------------------------------------------------------
    aw_hs     : out std_logic; -- AW handshake pulse
    w_hs      : out std_logic; -- W  handshake pulse
    b_hs      : out std_logic; -- B  handshake pulse
    w_partial : out std_logic; -- live WSTRB not all-1 on single-beat transfer
    w_last    : out std_logic; -- beat counter at zero

    ---------------------------------------------------------------------------
    -- Memory bus: write port
    ---------------------------------------------------------------------------
    mem_waddr  : out std_logic_vector(ADDR_WIDTH-1   downto 0);
    mem_wdata  : out std_logic_vector(DATA_WIDTH-1   downto 0);
    mem_wstrb  : out std_logic_vector(DATA_WIDTH/8-1 downto 0);
    mem_we     : out std_logic;  -- full-word write strobe
    mem_rmw_re : out std_logic;  -- RMW read  strobe (W_RMW_R)
    mem_rmw_we : out std_logic;  -- RMW write strobe (W_RMW_W)
    mem_wr_nowait : in std_logic; -- '1' = peripheral owns byte-enables

    ---------------------------------------------------------------------------
    -- RMW read-back data (from memory, valid in W_RMW_W)
    ---------------------------------------------------------------------------
    rmw_rdata : in std_logic_vector(DATA_WIDTH-1 downto 0)
  );
end entity axi_write_datapath;

architecture rtl of axi_write_datapath is

  constant BURST_INCR : std_logic_vector(1 downto 0) := "01";
  constant BURST_WRAP : std_logic_vector(1 downto 0) := "10";
  constant DATA_BYTES : natural := DATA_WIDTH / 8;
  constant STRB_ALL   : std_logic_vector(DATA_BYTES-1 downto 0) := (others => '1');

  -- Internal copies of registered outputs (needed combinationally before
  -- the output port is driven)
  signal waddr_q_i  : std_logic_vector(ADDR_WIDTH-1   downto 0);
  signal wdata_q_i  : std_logic_vector(DATA_WIDTH-1   downto 0);
  signal wstrb_q_i  : std_logic_vector(DATA_BYTES-1   downto 0);
  signal wid_q_i    : std_logic_vector(ID_WIDTH-1     downto 0);
  signal wlen_q_i   : std_logic_vector(7  downto 0);
  signal wburst_q_i : std_logic_vector(1  downto 0);
  signal werr_q_i   : std_logic_vector(0  downto 0);
  signal wcnt_q_i   : std_logic_vector(7  downto 0);

  signal wid_next               : std_logic_vector(ID_WIDTH-1   downto 0);
  signal waddr_next             : std_logic_vector(ADDR_WIDTH-1 downto 0);
  signal wlen_next              : std_logic_vector(7  downto 0);
  signal wburst_next            : std_logic_vector(1  downto 0);
  signal werr_next              : std_logic_vector(0  downto 0);
  signal wcnt_next              : std_logic_vector(7  downto 0);
  signal wdata_next             : std_logic_vector(DATA_WIDTH-1 downto 0);
  signal wstrb_next             : std_logic_vector(DATA_BYTES-1 downto 0);

  signal wid_en_s, waddr_en, wlen_en, wburst_en : std_logic;
  signal werr_en, wcnt_en, wdata_en, wstrb_en    : std_logic;

  signal aw_hs_i    : std_logic;
  signal w_hs_i     : std_logic;
  signal b_hs_i     : std_logic;
  signal bvalid_i   : std_logic;
  signal w_partial_i: std_logic;
  signal w_last_i   : std_logic;
  signal w_beat_i   : std_logic;
  signal w_full_beat_i      : std_logic;
  signal w_full_beat_cont_i : std_logic;

  signal waddr_inc  : std_logic_vector(ADDR_WIDTH-1 downto 0);
  signal wcnt_dec   : std_logic_vector(7  downto 0);

  signal rmw_b0, rmw_b1, rmw_b2, rmw_b3 : std_logic_vector(7 downto 0);
  signal rmw_merged_i : std_logic_vector(DATA_WIDTH-1 downto 0);

begin

  ---------------------------------------------------------------------------
  -- Handshakes
  ---------------------------------------------------------------------------
  aw_hs_i <= awvalid  and fsm_idle;
  w_hs_i  <= wvalid   and fsm_data;
  bvalid_i<= fsm_resp;
  b_hs_i  <= bvalid_i and bready;

  aw_hs <= aw_hs_i;
  w_hs  <= w_hs_i;
  b_hs  <= b_hs_i;

  ---------------------------------------------------------------------------
  -- Beat qualifiers
  ---------------------------------------------------------------------------
  w_partial_i <= '1' when wstrb /= STRB_ALL and wlen_q_i = x"00" else '0';
  w_last_i    <= '1' when wcnt_q_i = x"00"                        else '0';

  w_beat_i           <= fsm_data and w_hs_i;
  w_full_beat_i      <= fsm_data and w_hs_i and not w_partial_i;
  w_full_beat_cont_i <= '1' when fsm_data = '1' and w_hs_i = '1'
                                  and w_partial_i = '0' and w_last_i = '0'                    else '0';

  w_partial           <= w_partial_i;
  w_last              <= w_last_i;

  ---------------------------------------------------------------------------
  -- Address and count arithmetic
  ---------------------------------------------------------------------------
  waddr_inc <= std_logic_vector(unsigned(waddr_q_i) + DATA_BYTES)
                 when wburst_q_i = BURST_INCR else waddr_q_i;
  wcnt_dec  <= std_logic_vector(unsigned(wcnt_q_i) - 1);

  ---------------------------------------------------------------------------
  -- RMW byte merge
  -- Uses registered wdata_q_i / wstrb_q_i (latched on the W handshake)
  -- and rmw_rdata (presented by memory in W_RMW_W).
  ---------------------------------------------------------------------------
  rmw_b0 <= wdata_q_i(7  downto 0)  when wstrb_q_i(0) = '1' else rmw_rdata(7  downto 0);
  rmw_b1 <= wdata_q_i(15 downto 8)  when wstrb_q_i(1) = '1' else rmw_rdata(15 downto 8);
  rmw_b2 <= wdata_q_i(23 downto 16) when wstrb_q_i(2) = '1' else rmw_rdata(23 downto 16);
  rmw_b3 <= wdata_q_i(31 downto 24) when wstrb_q_i(3) = '1' else rmw_rdata(31 downto 24);
  rmw_merged_i <= rmw_b3 & rmw_b2 & rmw_b1 & rmw_b0;

  ---------------------------------------------------------------------------
  -- Register enables and next values
  ---------------------------------------------------------------------------
  wid_en_s  <= aw_hs_i;   wid_next    <= awid;
  wlen_en   <= aw_hs_i;   wlen_next   <= awlen;
  wburst_en <= aw_hs_i;   wburst_next <= awburst;
  werr_en   <= aw_hs_i;   werr_next(0)<= '1' when awburst = BURST_WRAP else '0';

  waddr_en   <= aw_hs_i or w_full_beat_cont_i;
  waddr_next <= awaddr when aw_hs_i = '1' else waddr_inc;

  wcnt_en   <= aw_hs_i or w_full_beat_cont_i;
  wcnt_next  <= awlen  when aw_hs_i = '1' else wcnt_dec;

  wdata_en   <= w_beat_i;  wdata_next <= wdata;
  wstrb_en   <= w_beat_i;  wstrb_next <= wstrb;

  ---------------------------------------------------------------------------
  -- Registers (vga_generic_register pattern)
  ---------------------------------------------------------------------------
  reg_wid    : entity work.vga_generic_register generic map(N => ID_WIDTH)
    port map(clk=>clk, reset=>rst, enable=>wid_en_s,  d=>wid_next,    q=>wid_q_i);
  reg_waddr  : entity work.vga_generic_register generic map(N => ADDR_WIDTH)
    port map(clk=>clk, reset=>rst, enable=>waddr_en,  d=>waddr_next,  q=>waddr_q_i);
  reg_wlen   : entity work.vga_generic_register generic map(N => 8)
    port map(clk=>clk, reset=>rst, enable=>wlen_en,   d=>wlen_next,   q=>wlen_q_i);
  reg_wburst : entity work.vga_generic_register generic map(N => 2)
    port map(clk=>clk, reset=>rst, enable=>wburst_en, d=>wburst_next, q=>wburst_q_i);
  reg_werr   : entity work.vga_generic_register generic map(N => 1)
    port map(clk=>clk, reset=>rst, enable=>werr_en,   d=>werr_next,   q=>werr_q_i);
  reg_wcnt   : entity work.vga_generic_register generic map(N => 8)
    port map(clk=>clk, reset=>rst, enable=>wcnt_en,   d=>wcnt_next,   q=>wcnt_q_i);
  reg_wdata  : entity work.vga_generic_register generic map(N => DATA_WIDTH)
    port map(clk=>clk, reset=>rst, enable=>wdata_en,  d=>wdata_next,  q=>wdata_q_i);
  reg_wstrb  : entity work.vga_generic_register generic map(N => DATA_BYTES)
    port map(clk=>clk, reset=>rst, enable=>wstrb_en,  d=>wstrb_next,  q=>wstrb_q_i);

  ---------------------------------------------------------------------------
  -- AXI write channel outputs
  ---------------------------------------------------------------------------
  awready <= fsm_idle;
  wready  <= fsm_data;
  bvalid  <= bvalid_i;
  bid     <= wid_q_i;
  bresp   <= "10" when werr_q_i(0) = '1' else "00";  -- SLVERR / OKAY

  ---------------------------------------------------------------------------
  -- Memory bus write outputs
  -- mem_wstrb: live wstrb in W_DATA (same cycle as mem_we);
  --            registered wstrb_q in W_RMW_W (AXI W channel no longer valid)
  -- mem_wdata: live wdata on full-word beats;
  --            merged word on RMW write
  ---------------------------------------------------------------------------
  mem_waddr  <= waddr_q_i;
  mem_wstrb  <= wstrb      when fsm_data = '1' else wstrb_q_i;
  mem_wdata  <= wdata      when fsm_data = '1' else rmw_merged_i;
  mem_we     <= w_beat_i and (not w_partial_i or mem_wr_nowait);
  mem_rmw_re <= fsm_rmw_r;
  mem_rmw_we <= fsm_rmw_w;


end architecture rtl;

