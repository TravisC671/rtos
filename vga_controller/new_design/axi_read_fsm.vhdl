--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- AXI Read Channel FSM                                          axi_read_fsm
--
-- Four-state Moore FSM controlling the AXI4 read channels (AR, R).
-- Instantiated by axi_slave; the datapath (axi_read_datapath) is a
-- separate entity driven by the one-hot state outputs.
--
-- States
-- ------
--   R_IDLE   Waiting for an address-channel handshake.
--            AR handshake (ar_hs)                                 -> R_REQ
--
--   R_REQ    Address captured; waiting for the write FSM to go idle
--            and for the memory to indicate readiness.
--              write_idle='1' and rd_nowait='1'                   -> R_SEND
--              write_idle='1' and rd_nowait='0'                   -> R_WAIT
--
--   R_WAIT   One-cycle pipeline bubble for memories with registered
--            output (rd_nowait='0'); unconditional                 -> R_SEND
--
--   R_SEND   Asserting RVALID.  On an R handshake (r_hs):
--              r_last='1'  last (or only) beat                     -> R_IDLE
--              r_last='0'  more beats remain                       -> R_REQ
--
-- Ports
-- -----
--   clk        clock
--   rst        synchronous active-high reset
--   ar_hs      AR handshake pulse  (ARVALID AND ARREADY)
--   r_hs       R  handshake pulse  (RVALID  AND RREADY)
--   r_last     '1' when the burst beat counter has reached zero
--   write_idle '1' when the write FSM is in W_IDLE (bus free)
--   rd_nowait  '1' when the peripheral returns data combinationally
--   in_idle .. in_send   one-hot current-state outputs
--
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use work.vga_pkg.all;

entity axi_read_fsm is
  port (
    clk        : in  std_logic;
    rst        : in  std_logic;
    ar_hs      : in  std_logic;
    r_hs       : in  std_logic;
    r_last     : in  std_logic;
    write_idle : in  std_logic;
    rd_nowait  : in  std_logic;
    in_idle    : out std_logic;
    in_req     : out std_logic;
    in_wait    : out std_logic;
    in_send    : out std_logic
  );
end entity axi_read_fsm;

architecture rtl of axi_read_fsm is

  type rstate_t is (R_IDLE, R_REQ, R_WAIT, R_SEND);
  signal rstate_q      : rstate_t;                    -- current state register
  signal rstate_next   : rstate_t;                    -- next state after reset mux
  signal rstate_next_i : rstate_t;                    -- next state before reset mux

  -- Per-state next-state intermediates: each holds the successor state
  -- that applies when the FSM is currently in that state.
  signal R_IDLE_next : rstate_t;
  signal R_REQ_next  : rstate_t;
  signal R_WAIT_next : rstate_t;
  signal R_SEND_next : rstate_t;

begin

  ---------------------------------------------------------------------------
  -- State register
  ---------------------------------------------------------------------------
  rstate_q <= rstate_next when rising_edge(clk);

  -- Reset mux: override with R_IDLE while rst is asserted.
  rstate_next <= R_IDLE when rst = '1' else rstate_next_i;

  -- Selected signal assignment dispatches to the appropriate per-state signal.
  with rstate_q select rstate_next_i <=
    R_IDLE_next when R_IDLE,
    R_REQ_next  when R_REQ,
    R_WAIT_next when R_WAIT,
    R_SEND_next when R_SEND,
    R_IDLE      when others;

  -- Per-state next-state combinational logic.
  -- Each signal computes the successor state assuming the FSM is in that state.

  -- R_IDLE: AR handshake starts a new transfer; otherwise hold.
  R_IDLE_next <= R_REQ when ar_hs = '1' else rstate_q;

  -- R_REQ: wait for write bus idle, then branch on rd_nowait; otherwise hold.
  -- Selector: write_idle & rd_nowait
  --   11  bus idle, no-wait peripheral   -> R_SEND
  --   10  bus idle, registered memory    -> R_WAIT
  --   0-  bus busy                       -> hold
  with sel2(write_idle, rd_nowait) select R_REQ_next <=
    R_SEND   when "11",
    R_WAIT   when "10",
    rstate_q when others;

  -- R_WAIT: unconditional single-cycle pipeline bubble advance to R_SEND.
  R_WAIT_next <= R_SEND;

  -- R_SEND: branch on r_hs & r_last; otherwise hold.
  -- Selector: r_hs & r_last
  --   11  handshake, last beat           -> R_IDLE
  --   10  handshake, more beats remain   -> R_REQ
  --   0-  no handshake                   -> hold
  with sel2(r_hs, r_last) select R_SEND_next <=
    R_IDLE   when "11",
    R_REQ    when "10",
    rstate_q when others;

  ---------------------------------------------------------------------------
  -- Outputs
  ---------------------------------------------------------------------------
  -- One-hot state decode for the datapath.
  in_idle <= '1' when rstate_q = R_IDLE else '0';
  in_req  <= '1' when rstate_q = R_REQ  else '0';
  in_wait <= '1' when rstate_q = R_WAIT else '0';
  in_send <= '1' when rstate_q = R_SEND else '0';

end architecture rtl;
