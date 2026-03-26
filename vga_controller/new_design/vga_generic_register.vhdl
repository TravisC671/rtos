--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- Generic N-bit Register                                   vga_generic_register
--
-- Synchronous, active-high reset, clock-enable register.
-- This is the fundamental storage primitive used throughout the VGA controller.
-- All sequential logic in the design (counters, FSM state, sync signals,
-- control registers, interrupt flags) is built from instances of this entity.
--
-- Behaviour
-- ---------
--   On every rising edge of clk:
--     reset = '1'  ->  q forced to all zeros (synchronous reset)
--     enable = '1' ->  q loaded from d
--     enable = '0' ->  q holds its current value
--
--   Reset takes priority over enable.  If both reset and enable are asserted
--   simultaneously, q is cleared to zero rather than loaded from d.
--
-- Generic
-- -------
--   N   bit width of the register (default 8)
--
-- Ports
-- -----
--   clk    clock input; register updates on rising edge
--   reset  synchronous active-high reset; clears q to all zeros
--   enable clock enable; when '0' the register holds its value
--   d      data input; captured when enable='1' and reset='0'
--   q      registered output
--
-- Implementation note
-- -------------------
-- The next-state mux (next_q_i) is a concurrent signal assignment evaluated
-- every delta cycle; the clocked assignment (q_i <= next_q_i when rising_edge)
-- captures the result on each rising edge.  This is the standard
-- process-free register idiom for VHDL-93 compatibility.
--------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;

entity vga_generic_register is
  generic(N: integer := 8);
  port(
    clk, reset, enable: in std_logic;
    d: in std_logic_vector(N-1 downto 0);
    q: out std_logic_vector(N-1 downto 0)
    );
end vga_generic_register;

architecture behavioral of vga_generic_register is
  signal q_i, next_q_i : std_logic_vector(N-1 downto 0); -- internal storage
begin
  -- Capture the next value on every rising clock edge.
  q_i <= next_q_i when rising_edge(clk);

  -- Next-value mux: reset > enable > hold.
  next_q_i <= (others => '0') when reset = '1' else  -- synchronous reset
              d                when enable = '1' else  -- load
              q_i;                                     -- hold

  -- Drive the output port from the internal registered value.
  q <= q_i;
end behavioral;
