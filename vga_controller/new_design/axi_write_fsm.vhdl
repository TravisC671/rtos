--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------


--------------------------------------------------------------------------------
-- AXI Write Channel FSM                                        axi_write_fsm
--
-- Five-state Moore FSM controlling the AXI4 write channels (AW, W, B).
-- Instantiated by axi_slave; the datapath (axi_write_datapath) is a
-- separate entity driven by the one-hot state outputs.
--
-- States
-- ------
--   W_IDLE   Waiting for an address-channel handshake.
--            AW handshake (aw_hs)                                 -> W_DATA
--
--   W_DATA   Accepting write data beats.  On a W handshake (w_hs):
--              wr_nowait='1'  peripheral owns byte-enables         -> W_RESP
--              w_partial='1'  partial beat, needs RMW              -> W_RMW_R
--              w_last='1'     full-word last beat                  -> W_RESP
--              w_last='0'     full-word non-last beat              -> W_DATA
--
--   W_RMW_R  Read-modify-write read phase.
--            One cycle to issue the read; unconditional            -> W_RMW_W
--
--   W_RMW_W  Read-modify-write write phase.
--            Merged word written; unconditional                    -> W_RESP
--
--   W_RESP   Asserting BVALID.
--            B handshake (b_hs)                                    -> W_IDLE
--
-- Ports
-- -----
--   clk        clock
--   rst        synchronous active-high reset
--   aw_hs      AW handshake pulse  (AWVALID AND AWREADY)
--   w_hs       W  handshake pulse  (WVALID  AND WREADY)
--   b_hs       B  handshake pulse  (BVALID  AND BREADY)
--   w_partial  '1' when WSTRB is not all-1 on a single-beat transfer
--   w_last     '1' when the burst beat counter has reached zero
--   wr_nowait  '1' when the peripheral handles byte-enables itself
--   in_idle .. in_resp   one-hot current-state outputs
--
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use work.vga_pkg.all;

entity axi_write_fsm is
  port (
    clk       : in  std_logic;
    rst       : in  std_logic;
    aw_hs     : in  std_logic;
    w_hs      : in  std_logic;
    b_hs      : in  std_logic;
    w_partial : in  std_logic;
    w_last    : in  std_logic;
    wr_nowait : in  std_logic;
    in_idle   : out std_logic;
    in_data   : out std_logic;
    in_rmw_r  : out std_logic;
    in_rmw_w  : out std_logic;
    in_resp   : out std_logic
    );
end entity axi_write_fsm;

architecture rtl of axi_write_fsm is
  type wstate_t is (W_IDLE, W_DATA, W_RMW_R, W_RMW_W, W_RESP);
  signal wstate_q      : wstate_t;                    -- current state register
  signal wstate_next   : wstate_t;                    -- next state after reset mux
  signal wstate_next_i : wstate_t;                    -- next state before reset mux

  -- Per-state next-state intermediates: each holds the successor state
  -- that applies when the FSM is currently in that state.
  signal W_IDLE_next    : wstate_t;
  signal W_DATA_next    : wstate_t;
  signal W_DATA_hs_next : wstate_t;  -- W_DATA successor when w_hs='1'
  signal w_data_sel     : t_sel3;    -- wr_nowait & w_partial & w_last selector
  signal W_RMW_R_next   : wstate_t;
  signal W_RMW_W_next   : wstate_t;
  signal W_RESP_next    : wstate_t;
begin

  ---------------------------------------------------------------------------
  -- State register
  -- Bare rising_edge assignment; rst='1' forces W_IDLE via wstate_next.
  ---------------------------------------------------------------------------
  wstate_q <= wstate_next when rising_edge(clk);

  -- Reset mux: override with W_IDLE while rst is asserted.
  wstate_next <= W_IDLE when rst = '1' else wstate_next_i;

  -- Selected signal assignment dispatches to the appropriate per-state signal.
  with wstate_q select wstate_next_i <=
    W_IDLE_next  when W_IDLE,
    W_DATA_next  when W_DATA,
    W_RMW_R_next when W_RMW_R,
    W_RMW_W_next when W_RMW_W,
    W_RESP_next  when W_RESP,
    W_IDLE       when others;

  ---------------------------------------------------------------------------
  -- Per-state next-state combinational logic
  ---------------------------------------------------------------------------
  -- W_IDLE: AW handshake starts a new transfer; otherwise hold.
  W_IDLE_next <= W_DATA when aw_hs = '1' else wstate_q;

  -- W_DATA: on a W handshake, decode wr_nowait & w_partial & w_last to
  -- select the next state; hold if no handshake this cycle.
  -- Selector encoding: wr_nowait & w_partial & w_last
  --   1--  wr_nowait: peripheral owns byte-enables, skip RMW    -> W_RESP
  --   01-  partial beat: sub-word write, RMW required            -> W_RMW_R
  --   001  full-word last beat: burst complete                   -> W_RESP
  --   000  full-word non-last beat: more beats to come           -> W_DATA
  w_data_sel <= sel3(wr_nowait, w_partial, w_last);
  with w_data_sel select W_DATA_hs_next <=
    W_RESP  when "100", W_RESP  when "101", W_RESP  when "110", W_RESP  when "111",
    W_RMW_R when "010", W_RMW_R when "011",
    W_RESP  when "001",
    W_DATA  when others;

  W_DATA_next <= W_DATA_hs_next when w_hs = '1' else wstate_q;

  -- W_RMW_R: unconditional single-cycle advance to W_RMW_W.
  W_RMW_R_next <= W_RMW_W;

  -- W_RMW_W: unconditional single-cycle advance to W_RESP.
  W_RMW_W_next <= W_RESP;

  -- W_RESP: B handshake completes the transaction; otherwise hold.
  W_RESP_next <= W_IDLE when b_hs = '1' else wstate_q;

  ---------------------------------------------------------------------------
  -- Outputs
  ---------------------------------------------------------------------------
  -- One-hot state decode for the datapath.
  in_idle  <= '1' when wstate_q = W_IDLE  else '0';
  in_data  <= '1' when wstate_q = W_DATA  else '0';
  in_rmw_r <= '1' when wstate_q = W_RMW_R else '0';
  in_rmw_w <= '1' when wstate_q = W_RMW_W else '0';
  in_resp  <= '1' when wstate_q = W_RESP  else '0';
end architecture rtl;
