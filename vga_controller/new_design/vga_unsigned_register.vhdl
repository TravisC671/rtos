--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- Unsigned Register                                      vga_unsigned_register
--
-- Thin wrapper around vga_generic_register that accepts and returns
-- ieee.numeric_std.unsigned operands, eliminating the std_logic_vector
-- conversion boilerplate in every caller.
--
-- This is used by vga_hv_counters so that each counter needs only two
-- signals (next-value and registered-value) rather than the four that the
-- slv-only vga_generic_register requires (_next, _d, _q, _int).
--
-- Generic
-- -------
--   N   bit width (default 10 for the 10-bit VGA counters)
--
-- Ports
-- -----
--   clk, reset, enable   standard register controls
--   d                    unsigned next value
--   q                    unsigned registered value
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity vga_unsigned_register is
  generic (N : integer := 10);
  port (
    clk    : in  std_logic;
    reset  : in  std_logic;
    enable : in  std_logic;
    d      : in  unsigned(N-1 downto 0);
    q      : out unsigned(N-1 downto 0)
  );
end entity vga_unsigned_register;

architecture rtl of vga_unsigned_register is

  signal d_slv : std_logic_vector(N-1 downto 0);
  signal q_slv : std_logic_vector(N-1 downto 0);

begin

  ---------------------------------------------------------------------------
  -- Global internal signals
  ---------------------------------------------------------------------------
  d_slv <= std_logic_vector(d);

  ---------------------------------------------------------------------------
  -- Register
  ---------------------------------------------------------------------------
  reg_inst : entity work.vga_generic_register
    generic map (N => N)
    port map (
      clk    => clk,
      reset  => reset,
      enable => enable,
      d      => d_slv,
      q      => q_slv
    );

  ---------------------------------------------------------------------------
  -- Outputs
  ---------------------------------------------------------------------------
  q <= unsigned(q_slv);

end architecture rtl;
