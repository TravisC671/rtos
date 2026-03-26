--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity axi_read_datapath is
  generic (
    ADDR_WIDTH : integer := 32;
    DATA_WIDTH : integer := 32;
    ID_WIDTH   : integer := 4
  );
  port (
    clk  : in std_logic;
    rst  : in std_logic;

    ---------------------------------------------------------------------------
    -- AXI4 Read Address Channel (slave)
    ---------------------------------------------------------------------------
    arid    : in  std_logic_vector(ID_WIDTH-1   downto 0);
    araddr  : in  std_logic_vector(ADDR_WIDTH-1 downto 0);
    arlen   : in  std_logic_vector(7  downto 0);
    arsize  : in  std_logic_vector(2  downto 0);
    arburst : in  std_logic_vector(1  downto 0);
    arvalid : in  std_logic;
    arready : out std_logic;   -- rfsm_idle AND wfsm_idle (driven from outside)

    ---------------------------------------------------------------------------
    -- AXI4 Read Data Channel (slave)
    ---------------------------------------------------------------------------
    rid    : out std_logic_vector(ID_WIDTH-1   downto 0);
    rdata  : out std_logic_vector(DATA_WIDTH-1 downto 0);
    rresp  : out std_logic_vector(1 downto 0);
    rlast  : out std_logic;
    rvalid : out std_logic;
    rready : in  std_logic;

    ---------------------------------------------------------------------------
    -- FSM one-hot state inputs
    ---------------------------------------------------------------------------
    fsm_idle  : in std_logic;  -- R_IDLE
    fsm_req   : in std_logic;  -- R_REQ  : mem_re pulsed
    fsm_wait  : in std_logic;  -- R_WAIT : pipeline bubble
    fsm_send  : in std_logic;  -- R_SEND : RVALID asserted
    wfsm_idle : in std_logic;  -- write FSM W_IDLE (bus arbitration)

    ---------------------------------------------------------------------------
    -- Handshake / qualifier outputs (fed back to FSM)
    ---------------------------------------------------------------------------
    ar_hs  : out std_logic;  -- AR handshake pulse
    r_hs   : out std_logic;  -- R  handshake pulse
    r_last : out std_logic;  -- beat counter at zero

    ---------------------------------------------------------------------------
    -- ARREADY input: driven by parent as (fsm_idle AND wfsm_idle)
    -- We expose it as a port so the parent can gate it with write_idle without
    -- the datapath needing to know about the write FSM directly.
    ---------------------------------------------------------------------------
    arready_in : in std_logic;

    ---------------------------------------------------------------------------
    -- Read data capture: parent controls source mux and enable
    ---------------------------------------------------------------------------
    cap_en   : in std_logic;                              -- load rdata_cap
    cap_data : in std_logic_vector(DATA_WIDTH-1 downto 0); -- data to capture

    ---------------------------------------------------------------------------
    -- Memory bus: read port
    ---------------------------------------------------------------------------
    mem_raddr : out std_logic_vector(ADDR_WIDTH-1 downto 0);
    mem_re    : out std_logic;  -- one-cycle read strobe

    ---------------------------------------------------------------------------
    -- Derived condition outputs for parent use
    ---------------------------------------------------------------------------
    r_req_arb_won : out std_logic   -- R_REQ with write bus idle
  );
end entity axi_read_datapath;

architecture rtl of axi_read_datapath is

  constant BURST_INCR : std_logic_vector(1 downto 0) := "01";
  constant BURST_WRAP : std_logic_vector(1 downto 0) := "10";
  constant DATA_BYTES : natural := DATA_WIDTH / 8;

  signal rid_q_i    : std_logic_vector(ID_WIDTH-1   downto 0);
  signal raddr_q_i  : std_logic_vector(ADDR_WIDTH-1 downto 0);
  signal rlen_q_i   : std_logic_vector(7  downto 0);
  signal rsize_q_i  : std_logic_vector(2  downto 0);
  signal rburst_q_i : std_logic_vector(1  downto 0);
  signal rerr_q_i   : std_logic_vector(0  downto 0);
  signal rcnt_q_i   : std_logic_vector(7  downto 0);
  signal rdata_cap_i: std_logic_vector(DATA_WIDTH-1 downto 0);

  signal rid_en, raddr_en, rlen_en, rsize_en   : std_logic;
  signal rburst_en, rerr_en, rcnt_en            : std_logic;

  signal rid_next    : std_logic_vector(ID_WIDTH-1   downto 0);
  signal raddr_next  : std_logic_vector(ADDR_WIDTH-1 downto 0);
  signal rlen_next   : std_logic_vector(7  downto 0);
  signal rsize_next  : std_logic_vector(2  downto 0);
  signal rburst_next : std_logic_vector(1  downto 0);
  signal rerr_next   : std_logic_vector(0  downto 0);
  signal rcnt_next   : std_logic_vector(7  downto 0);

  signal ar_hs_i       : std_logic;
  signal r_hs_i        : std_logic;
  signal r_last_i      : std_logic;
  signal rvalid_i      : std_logic;
  signal r_req_arb_won_i : std_logic;
  signal r_beat_cont_i   : std_logic;
  signal r_subword_byte  : std_logic;
  signal r_subword_half  : std_logic;

  signal raddr_inc : std_logic_vector(ADDR_WIDTH-1 downto 0);
  signal rcnt_dec  : std_logic_vector(7  downto 0);
  signal rdata_sel : std_logic_vector(3 downto 0);  -- sub-word mux selector
  signal rdata_out : std_logic_vector(DATA_WIDTH-1 downto 0);

begin

  ---------------------------------------------------------------------------
  -- Handshakes
  ---------------------------------------------------------------------------
  ar_hs_i  <= arvalid  and arready_in;
  rvalid_i <= fsm_send;
  r_hs_i   <= rvalid_i and rready;

  ar_hs  <= ar_hs_i;
  r_hs   <= r_hs_i;

  ---------------------------------------------------------------------------
  -- Beat qualifiers
  ---------------------------------------------------------------------------
  r_last_i       <= '1' when rcnt_q_i = x"00" else '0';
  r_req_arb_won_i<= fsm_req and wfsm_idle;
  r_beat_cont_i  <= fsm_send and r_hs_i and not r_last_i;
  r_subword_byte <= '1' when rlen_q_i = x"00" and rsize_q_i = "000" else '0';
  r_subword_half <= '1' when rlen_q_i = x"00" and rsize_q_i = "001" else '0';

  r_last        <= r_last_i;
  r_req_arb_won <= r_req_arb_won_i;

  ---------------------------------------------------------------------------
  -- Address and count arithmetic
  ---------------------------------------------------------------------------
  raddr_inc <= std_logic_vector(unsigned(raddr_q_i) + DATA_BYTES)
                 when rburst_q_i = BURST_INCR else raddr_q_i;
  rcnt_dec  <= std_logic_vector(unsigned(rcnt_q_i) - 1);

  ---------------------------------------------------------------------------
  -- Register enables and next values
  ---------------------------------------------------------------------------
  rid_en    <= ar_hs_i;  rid_next    <= arid;
  rlen_en   <= ar_hs_i;  rlen_next   <= arlen;
  rsize_en  <= ar_hs_i;  rsize_next  <= arsize;
  rburst_en <= ar_hs_i;  rburst_next <= arburst;
  rerr_en   <= ar_hs_i;  rerr_next(0)<= '1' when arburst = BURST_WRAP else '0';

  raddr_en   <= ar_hs_i or r_beat_cont_i;
  raddr_next <= araddr when ar_hs_i = '1' else raddr_inc;

  rcnt_en   <= ar_hs_i or r_beat_cont_i;
  rcnt_next  <= arlen  when ar_hs_i = '1' else rcnt_dec;

  ---------------------------------------------------------------------------
  -- Registers
  ---------------------------------------------------------------------------
  reg_rid      : entity work.vga_generic_register generic map(N => ID_WIDTH)
    port map(clk=>clk, reset=>rst, enable=>rid_en,    d=>rid_next,    q=>rid_q_i);
  reg_raddr    : entity work.vga_generic_register generic map(N => ADDR_WIDTH)
    port map(clk=>clk, reset=>rst, enable=>raddr_en,  d=>raddr_next,  q=>raddr_q_i);
  reg_rlen     : entity work.vga_generic_register generic map(N => 8)
    port map(clk=>clk, reset=>rst, enable=>rlen_en,   d=>rlen_next,   q=>rlen_q_i);
  reg_rsize    : entity work.vga_generic_register generic map(N => 3)
    port map(clk=>clk, reset=>rst, enable=>rsize_en,  d=>rsize_next,  q=>rsize_q_i);
  reg_rburst   : entity work.vga_generic_register generic map(N => 2)
    port map(clk=>clk, reset=>rst, enable=>rburst_en, d=>rburst_next, q=>rburst_q_i);
  reg_rerr     : entity work.vga_generic_register generic map(N => 1)
    port map(clk=>clk, reset=>rst, enable=>rerr_en,   d=>rerr_next,   q=>rerr_q_i);
  reg_rcnt     : entity work.vga_generic_register generic map(N => 8)
    port map(clk=>clk, reset=>rst, enable=>rcnt_en,   d=>rcnt_next,   q=>rcnt_q_i);
  reg_rdata_cap: entity work.vga_generic_register generic map(N => DATA_WIDTH)
    port map(clk=>clk, reset=>rst, enable=>cap_en,    d=>cap_data,    q=>rdata_cap_i);

  ---------------------------------------------------------------------------
  -- Sub-word extraction (applied in R_SEND)
  --
  -- Selector encoding: r_subword_byte & r_subword_half & raddr_q_i(1) & raddr_q_i(0)
  --   "1000"  byte lane 0    (byte, addr[1:0]=00)
  --   "1001"  byte lane 1    (byte, addr[1:0]=01)
  --   "1010"  byte lane 2    (byte, addr[1:0]=10)
  --   "1011"  byte lane 3    (byte, addr[1:0]=11)
  --   "0100"  lower halfword (half, addr[1]=0)
  --   "0110"  upper halfword (half, addr[1]=1)
  --   others  full word
  ---------------------------------------------------------------------------
  rdata_sel <= r_subword_byte & r_subword_half & raddr_q_i(1) & raddr_q_i(0);

  with rdata_sel select rdata_out <=
    (x"000000" & rdata_cap_i( 7 downto  0)) when "1000",
    (x"000000" & rdata_cap_i(15 downto  8)) when "1001",
    (x"000000" & rdata_cap_i(23 downto 16)) when "1010",
    (x"000000" & rdata_cap_i(31 downto 24)) when "1011",
    (x"0000"   & rdata_cap_i(15 downto  0)) when "0100",
    (x"0000"   & rdata_cap_i(31 downto 16)) when "0110",
    rdata_cap_i                              when others;

  ---------------------------------------------------------------------------
  -- AXI read channel outputs
  ---------------------------------------------------------------------------
  arready <= arready_in;
  rvalid  <= rvalid_i;
  rlast   <= fsm_send and r_last_i;
  rid     <= rid_q_i;
  rdata   <= rdata_out;
  rresp   <= "10" when rerr_q_i(0) = '1' else "00";  -- SLVERR / OKAY

  ---------------------------------------------------------------------------
  -- Memory bus read output
  ---------------------------------------------------------------------------
  mem_raddr <= raddr_q_i;
  mem_re    <= r_req_arb_won_i;


end architecture rtl;
