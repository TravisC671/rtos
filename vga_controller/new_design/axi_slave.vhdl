--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- AXI4 Slave Controller                                              axi_slave
--
-- Structural wrapper connecting the shared write/read FSMs and the shared
-- write/read datapaths.  Contains no logic of its own beyond port connections.
--
-- AXI4 interface
-- --------------
-- The slave port uses vga_pkg record types:
--   s_axi_in  : in  t_axi4_m2s   master-to-slave signals
--   s_axi_out : out t_axi4_s2m   slave-to-master signals
--
-- The records are fixed at the design maximums (32-bit addr, 32-bit data,
-- 4-bit ID).  The ADDR_WIDTH, DATA_WIDTH, and ID_WIDTH generics control the
-- actual datapath widths; connections to sub-entities use the lower bits of
-- the record fields and zero-extend outputs back to 32 bits.
--
-- Memory bus interface
-- --------------------
-- The internal memory bus uses vga_pkg record types:
--   mem_wr_cmd  : out t_mem_wr_cmd    write command to arbiter
--   mem_wr_resp : in  t_mem_wr_resp   write response from arbiter
--   mem_rd_cmd  : out t_mem_rd_cmd    read command to arbiter
--   mem_rd_resp : in  t_mem_rd_resp   read response from arbiter
--
-- Hierarchy (compile order)
-- -------------------------
--   axi_channel_fsm.vhdl   axi_write_fsm, axi_read_fsm
--   axi_datapath.vhdl      axi_write_datapath, axi_read_datapath
--   axi_slave.vhdl         axi_slave  (this file, structural only)
--
-- Design rules: no process statements; all concurrent signal assignments.
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.vga_pkg.all;

entity axi_slave is
  generic (
    ADDR_WIDTH : integer := 32;
    DATA_WIDTH : integer := 32;
    ID_WIDTH   : integer := 4
  );
  port (
    axi_clk    : in  std_logic;
    aresetn : in  std_logic;

    s_axi_in  : in  t_axi4_m2s;
    s_axi_out : out t_axi4_s2m;

    mem_wr_cmd  : out t_mem_wr_cmd;
    mem_wr_resp : in  t_mem_wr_resp;
    mem_rd_cmd  : out t_mem_rd_cmd;
    mem_rd_resp : in  t_mem_rd_resp
  );
end entity axi_slave;

architecture rtl of axi_slave is

  constant DATA_BYTES : integer := DATA_WIDTH / 8;

  signal rst : std_logic;

  -- Write FSM state outputs
  signal wfsm_idle  : std_logic;
  signal wfsm_data  : std_logic;
  signal wfsm_rmw_r : std_logic;
  signal wfsm_rmw_w : std_logic;
  signal wfsm_resp  : std_logic;

  -- Write datapath handshake signals
  signal aw_hs      : std_logic;
  signal w_hs       : std_logic;
  signal b_hs       : std_logic;
  signal w_partial  : std_logic;
  signal w_last     : std_logic;

  -- Read FSM state outputs
  signal rfsm_idle  : std_logic;
  signal rfsm_req   : std_logic;
  signal rfsm_wait  : std_logic;
  signal rfsm_send  : std_logic;

  -- Read datapath handshake signals
  signal ar_hs      : std_logic;
  signal r_hs       : std_logic;
  signal r_last     : std_logic;

  -- Internal readable copies of slave-driven ready signals
  signal awready_i     : std_logic;
  signal arready_i     : std_logic;
  signal cap_en        : std_logic;
  signal r_req_arb_won : std_logic;

  -- Parameterised-width intermediates:
  -- records are fixed 32-bit; datapaths are ADDR_WIDTH/DATA_WIDTH/ID_WIDTH.
  signal awid_i          : std_logic_vector(ID_WIDTH-1   downto 0);
  signal awaddr_i        : std_logic_vector(ADDR_WIDTH-1 downto 0);
  signal wdata_i         : std_logic_vector(DATA_WIDTH-1 downto 0);
  signal wstrb_i         : std_logic_vector(DATA_BYTES-1 downto 0);
  signal bid_i           : std_logic_vector(ID_WIDTH-1   downto 0);
  signal arid_i          : std_logic_vector(ID_WIDTH-1   downto 0);
  signal araddr_i        : std_logic_vector(ADDR_WIDTH-1 downto 0);
  signal rid_i           : std_logic_vector(ID_WIDTH-1   downto 0);
  signal rdata_i         : std_logic_vector(DATA_WIDTH-1 downto 0);
  signal mem_waddr_i     : std_logic_vector(ADDR_WIDTH-1 downto 0);
  signal mem_wdata_i     : std_logic_vector(DATA_WIDTH-1 downto 0);
  signal mem_wstrb_i     : std_logic_vector(DATA_BYTES-1 downto 0);
  signal mem_rmw_rdata_i : std_logic_vector(DATA_WIDTH-1 downto 0);
  signal mem_raddr_i     : std_logic_vector(ADDR_WIDTH-1 downto 0);
  signal mem_rdata_i     : std_logic_vector(DATA_WIDTH-1 downto 0);

begin

  ---------------------------------------------------------------------------
  -- Global internal signals
  ---------------------------------------------------------------------------
  rst       <= not aresetn;
  arready_i <= rfsm_idle and wfsm_idle;
  cap_en    <= rfsm_wait or (r_req_arb_won and mem_rd_resp.nowait);

  -- Slice record inputs down to datapath widths
  awid_i          <= s_axi_in.awid(ID_WIDTH-1    downto 0);
  awaddr_i        <= s_axi_in.awaddr(ADDR_WIDTH-1 downto 0);
  wdata_i         <= s_axi_in.wdata(DATA_WIDTH-1  downto 0);
  wstrb_i         <= s_axi_in.wstrb(DATA_BYTES-1  downto 0);
  arid_i          <= s_axi_in.arid(ID_WIDTH-1    downto 0);
  araddr_i        <= s_axi_in.araddr(ADDR_WIDTH-1 downto 0);
  mem_rmw_rdata_i <= mem_wr_resp.rmw_rdata(DATA_WIDTH-1 downto 0);
  mem_rdata_i     <= mem_rd_resp.data(DATA_WIDTH-1 downto 0);

  -- Zero-pad datapath outputs back to record widths
  s_axi_out.awready <= awready_i;
  s_axi_out.arready <= arready_i;
  s_axi_out.bid     <= std_logic_vector(resize(unsigned(bid_i),    4));
  s_axi_out.rid     <= std_logic_vector(resize(unsigned(rid_i),    4));
  s_axi_out.rdata   <= std_logic_vector(resize(unsigned(rdata_i), 32));
  mem_wr_cmd.addr   <= std_logic_vector(resize(unsigned(mem_waddr_i), 32));
  mem_wr_cmd.data   <= std_logic_vector(resize(unsigned(mem_wdata_i), 32));
  mem_wr_cmd.strb   <= std_logic_vector(resize(unsigned(mem_wstrb_i),  4));
  mem_rd_cmd.addr   <= std_logic_vector(resize(unsigned(mem_raddr_i), 32));

  ---------------------------------------------------------------------------
  -- Write FSM
  ---------------------------------------------------------------------------
  write_fsm_inst : entity work.axi_write_fsm
    port map (
      clk       => axi_clk,       rst      => rst,
      aw_hs     => aw_hs,      w_hs     => w_hs,      b_hs     => b_hs,
      w_partial => w_partial,  w_last   => w_last,
      wr_nowait => mem_wr_resp.nowait,
      in_idle   => wfsm_idle,  in_data  => wfsm_data,
      in_rmw_r  => wfsm_rmw_r, in_rmw_w => wfsm_rmw_w, in_resp => wfsm_resp
    );

  ---------------------------------------------------------------------------
  -- Read FSM
  ---------------------------------------------------------------------------
  read_fsm_inst : entity work.axi_read_fsm
    port map (
      clk        => axi_clk,      rst       => rst,
      ar_hs      => ar_hs,     r_hs      => r_hs,     r_last     => r_last,
      write_idle => wfsm_idle, rd_nowait => mem_rd_resp.nowait,
      in_idle    => rfsm_idle, in_req    => rfsm_req,
      in_wait    => rfsm_wait, in_send   => rfsm_send
    );

  ---------------------------------------------------------------------------
  -- Write datapath
  ---------------------------------------------------------------------------
  write_dp_inst : entity work.axi_write_datapath
    generic map (ADDR_WIDTH => ADDR_WIDTH, DATA_WIDTH => DATA_WIDTH,
                 ID_WIDTH   => ID_WIDTH)
    port map (
      clk           => axi_clk,              rst           => rst,
      awid          => awid_i,            awaddr        => awaddr_i,
      awlen         => s_axi_in.awlen,    awburst       => s_axi_in.awburst,
      awvalid       => s_axi_in.awvalid,  awready       => awready_i,
      wdata         => wdata_i,           wstrb         => wstrb_i,
      wvalid        => s_axi_in.wvalid,   wready        => s_axi_out.wready,
      bid           => bid_i,             bresp         => s_axi_out.bresp,
      bvalid        => s_axi_out.bvalid,  bready        => s_axi_in.bready,
      fsm_idle      => wfsm_idle,         fsm_data      => wfsm_data,
      fsm_rmw_r     => wfsm_rmw_r,        fsm_rmw_w     => wfsm_rmw_w,
      fsm_resp      => wfsm_resp,
      aw_hs         => aw_hs,             w_hs          => w_hs,
      b_hs          => b_hs,              w_partial     => w_partial,
      w_last        => w_last,
      mem_waddr     => mem_waddr_i,       mem_wdata     => mem_wdata_i,
      mem_wstrb     => mem_wstrb_i,       mem_we        => mem_wr_cmd.we,
      mem_rmw_re    => mem_wr_cmd.rmw_re, mem_rmw_we    => mem_wr_cmd.rmw_we,
      mem_wr_nowait => mem_wr_resp.nowait, rmw_rdata    => mem_rmw_rdata_i
    );

  ---------------------------------------------------------------------------
  -- Read datapath
  ---------------------------------------------------------------------------
  read_dp_inst : entity work.axi_read_datapath
    generic map (ADDR_WIDTH => ADDR_WIDTH, DATA_WIDTH => DATA_WIDTH,
                 ID_WIDTH   => ID_WIDTH)
    port map (
      clk           => axi_clk,              rst           => rst,
      arid          => arid_i,            araddr        => araddr_i,
      arlen         => s_axi_in.arlen,    arsize        => s_axi_in.arsize,
      arburst       => s_axi_in.arburst,  arvalid       => s_axi_in.arvalid,
      arready       => s_axi_out.arready, arready_in    => arready_i,
      rid           => rid_i,             rdata         => rdata_i,
      rresp         => s_axi_out.rresp,   rlast         => s_axi_out.rlast,
      rvalid        => s_axi_out.rvalid,  rready        => s_axi_in.rready,
      fsm_idle      => rfsm_idle,         fsm_req       => rfsm_req,
      fsm_wait      => rfsm_wait,         fsm_send      => rfsm_send,
      wfsm_idle     => wfsm_idle,
      ar_hs         => ar_hs,             r_hs          => r_hs,
      r_last        => r_last,
      cap_en        => cap_en,            cap_data      => mem_rdata_i,
      mem_raddr     => mem_raddr_i,       mem_re        => mem_rd_cmd.re,
      r_req_arb_won => r_req_arb_won
    );

end architecture rtl;
