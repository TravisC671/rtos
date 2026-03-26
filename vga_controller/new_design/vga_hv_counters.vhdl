--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- VGA Horizontal / Vertical Counter FSM                        vga_hv_counters
--
-- Self-contained raster scan counter pair.  h_count increments every pixel
-- clock and wraps at H_TOTAL-1; v_count increments at the end of each
-- horizontal line and wraps at V_TOTAL-1.
--
-- The look-ahead outputs h_count_next and v_count_next expose the values the
-- counters will hold on the next clock edge.  The framebuffer BRAM has a
-- one-cycle registered output, so the parent (vga_timing_gen) must present
-- fb_addr one cycle ahead of the pixel that will appear on screen.  Providing
-- the look-ahead here avoids replicating the next-state combinational logic
-- in the parent.
--
-- The derived h_at_end signal is exposed so the parent can gate
-- vertical-counter and sync logic without re-evaluating the comparison.
--
-- Timing constants (H_TOTAL, V_TOTAL) come from vga_pkg.
--
-- Each counter is stored in a vga_unsigned_register instance; this removes
-- the std_logic_vector conversion when wrapping vga_generic_register directly.
--
-- Ports
-- -----
--   pixel_clk     25.175 MHz pixel clock (or close to it)
--   reset         active-high synchronous reset
--   h_count       current horizontal pixel position  (0 .. H_TOTAL-1)
--   v_count       current vertical  line   position  (0 .. V_TOTAL-1)
--   h_at_end      '1' when h_count = H_TOTAL-1
--   h_count_next  combinational next value of h_count
--   v_count_next  combinational next value of v_count
--
-- Design rules: no process statements; all concurrent signal assignments.
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.vga_pkg.all;

entity vga_hv_counters is
  port (
    pixel_clk    : in  std_logic;
    reset        : in  std_logic;
    h_count      : out unsigned(9 downto 0);
    v_count      : out unsigned(9 downto 0);
    h_at_end     : out std_logic;
    h_count_next : out unsigned(9 downto 0);
    v_count_next : out unsigned(9 downto 0)
  );
end entity vga_hv_counters;

architecture rtl of vga_hv_counters is

  signal h_q        : unsigned(9 downto 0);
  signal h_next     : unsigned(9 downto 0);
  signal h_at_end_i : std_logic;

  signal v_q        : unsigned(9 downto 0);
  signal v_next     : unsigned(9 downto 0);

begin

  ---------------------------------------------------------------------------
  -- Global internal signals
  ---------------------------------------------------------------------------
  h_at_end_i <= '1' when h_q = H_TOTAL - 1 else '0';

  ---------------------------------------------------------------------------
  -- Horizontal counter
  -- Increments every pixel clock; wraps to zero at H_TOTAL-1.
  ---------------------------------------------------------------------------
  reg_h : entity work.vga_unsigned_register
    generic map (N => 10)
    port map (clk => pixel_clk, reset => reset, enable => '1',
              d   => h_next,    q     => h_q);

  h_next <= (others => '0') when h_at_end_i = '1' else
            h_q + 1;

  ---------------------------------------------------------------------------
  -- Vertical counter
  -- Increments once per line; wraps to zero at V_TOTAL-1.
  ---------------------------------------------------------------------------
  reg_v : entity work.vga_unsigned_register
    generic map (N => 10)
    port map (clk => pixel_clk, reset => reset, enable => '1',
              d   => v_next,    q     => v_q);

  v_next <= (others => '0') when h_at_end_i = '1' and v_q = V_TOTAL - 1 else
            v_q + 1          when h_at_end_i = '1' else
            v_q;

  ---------------------------------------------------------------------------
  -- Outputs
  ---------------------------------------------------------------------------
  h_count      <= h_q;
  v_count      <= v_q;
  h_at_end     <= h_at_end_i;
  h_count_next <= h_next;
  v_count_next <= v_next;

end architecture rtl;
