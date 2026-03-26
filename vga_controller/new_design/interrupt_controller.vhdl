--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- Interrupt Controller                                     interrupt_controller
--
-- Generic edge-triggered interrupt controller with write-1-to-clear.
--
-- Features
-- --------
--   Detects rising edge of event signal.
--   Sets pending flag on event.
--   Generates IRQ when enabled and pending.
--   Write-1-to-clear mechanism for pending flag.
--   Fully synchronous design; no process statements.
--
-- Typical usage
-- -------------
--   event:  Connect to VSYNC, frame done, UART RX, etc.
--   enable: From interrupt enable register bit.
--   clear:  From AXI write with W1C bit.
--   irq:    Connect to interrupt output pin or processor IRQ input.
--
-- All flip-flops are vga_generic_register instances (active-high reset).
-- rst is derived from the active-low resetn port by inversion.
-- vga_generic_register d/q ports are std_logic_vector, so each std_logic
-- value is wrapped into slv(0 downto 0) on input and unwrapped on output.
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

entity interrupt_controller is
  port (
    clk    : in  std_logic;
    resetn : in  std_logic;

    event  : in  std_logic;   -- rising edge triggers interrupt
    enable : in  std_logic;   -- interrupt enable (1 = enabled)
    clear  : in  std_logic;   -- write-1-to-clear pending flag

    pending : out std_logic;  -- interrupt pending flag (read-only status)
    irq     : out std_logic   -- interrupt request (enable AND pending)
  );
end entity interrupt_controller;

architecture rtl of interrupt_controller is

  signal rst : std_logic;

  -- event_prev register: samples event every cycle for rising-edge detection
  signal event_prev_d : std_logic_vector(0 downto 0);
  signal event_prev_q : std_logic_vector(0 downto 0);
  signal event_prev   : std_logic;

  -- pending_flag register: set on rising edge of event; cleared by W1C write
  signal pending_next   : std_logic;
  signal pending_flag_d : std_logic_vector(0 downto 0);
  signal pending_flag_q : std_logic_vector(0 downto 0);
  signal pending_flag   : std_logic;

begin

  ---------------------------------------------------------------------------
  -- Global internal signals
  ---------------------------------------------------------------------------
  rst <= not resetn;

  ---------------------------------------------------------------------------
  -- event_prev register
  -- Tracks the previous-cycle value of event for rising-edge detection.
  ---------------------------------------------------------------------------
  reg_event_prev : entity work.vga_generic_register
    generic map (N => 1)
    port map (clk => clk, reset => rst, enable => '1',
              d   => event_prev_d, q => event_prev_q);

  event_prev_d(0) <= event;
  event_prev      <= event_prev_q(0);

  ---------------------------------------------------------------------------
  -- pending_flag register
  -- Set on rising edge of event; cleared by W1C write; reset clears.
  ---------------------------------------------------------------------------
  reg_pending_flag : entity work.vga_generic_register
    generic map (N => 1)
    port map (clk => clk, reset => rst, enable => '1',
              d   => pending_flag_d, q => pending_flag_q);

  pending_next    <= '0' when clear  = '1'                      else
                     '1' when event  = '1' and event_prev = '0' else
                     pending_flag;
  pending_flag_d(0) <= pending_next;
  pending_flag      <= pending_flag_q(0);

  ---------------------------------------------------------------------------
  -- Outputs
  ---------------------------------------------------------------------------
  pending <= pending_flag;
  irq     <= enable and pending_flag;

end architecture rtl;
