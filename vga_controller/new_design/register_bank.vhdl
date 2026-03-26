--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- VGA Register Bank                                               register_bank
--
-- Four 32-bit AXI-accessible registers.  Only bit 0 of each register is
-- implemented in hardware; bits [31:1] are RAZ/WI.
--
-- Each implemented bit is stored in a single vga_generic_register (N=1).
-- The rdata port concatenates the stored bit with 31 zeros, so unimplemented
-- bits always read back as zero regardless of what was written.
--
-- Register map (word offsets from the register bank base address)
-- --------------------------------------------------------------
--   word 0  Control Register      bit 0 = MODE   (R/W)
--   word 1  Display Buffer Reg    bit 0 = BUF    (R/W)
--   word 2  IRQ Enable Register   bit 0 = VSIEN  (R/W)
--   word 3  IRQ Clear/Status Reg  bit 0 = VSIA   (R/W1C, hardware-set)
--
-- VSIA (word 3 bit 0) is owned by interrupt_controller.  Its current value
-- arrives on vsia_pending every cycle and is re-registered here so the
-- register read always returns the live status.  A software write of 1 to
-- bit 0 generates a one-cycle vsia_clear pulse consumed by interrupt_controller.
--
-- Write rules
-- -----------
--   Bits [31:1]  WI  — writes ignored; only bit 0 register is connected
--   Bit  0       W   — written when we='1', addr=N, and wstrb(0)='1'
--   VSIA bit 0   W1C — clear pulse generated when above conditions hold
--                      and wdata(0)='1'; hardware always wins on the same cycle
--
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

entity register_bank is
  port (
    clk    : in  std_logic;
    resetn : in  std_logic;

    -- Software access port (from axi_mem_arbiter)
    addr   : in  integer range 0 to 3;
    we     : in  std_logic;
    wstrb  : in  std_logic_vector(3 downto 0);
    wdata  : in  std_logic_vector(31 downto 0);
    rdata  : out std_logic_vector(31 downto 0);

    -- Named control outputs
    mode       : out std_logic;  -- Control[0]:  0=640x400, 1=320x200
    buf_select : out std_logic;  -- DispBuf[0]:  0=buf0,    1=buf1
    vsien      : out std_logic;  -- IRQEn[0]:    interrupt enable

    -- VSIA W1C interface
    vsia_clear   : out std_logic;  -- one-cycle pulse when SW writes 1 to word 3 bit 0
    vsia_pending : in  std_logic   -- live pending flag from interrupt_controller
  );
end entity register_bank;

architecture rtl of register_bank is

  signal rst : std_logic;

  -- Per-register write enables (word select AND byte-lane 0 enable).
  -- we_gate factors out the common (we AND wstrb(0)) condition.
  signal we_gate        : std_logic;
  signal we0, we1, we2, we3 : std_logic;

  -- Stored bit-0 values (one 1-bit register per word),
  -- packed into std_logic_vector(0 downto 0) for vga_generic_register.
  signal mode_d,  mode_q  : std_logic_vector(0 downto 0);
  signal buf_d,   buf_q   : std_logic_vector(0 downto 0);
  signal vsien_d, vsien_q : std_logic_vector(0 downto 0);
  signal vsia_d,  vsia_q  : std_logic_vector(0 downto 0);

begin

  ---------------------------------------------------------------------------
  -- Global internal signals
  ---------------------------------------------------------------------------
  rst     <= not resetn;
  we_gate <= we and wstrb(0);

  -- Write enables: four-input mux on addr selects which register is written.
  -- Bit 0 lives in byte lane 0; we_gate already incorporates the wstrb gate.
  with addr select we0 <= we_gate when 0, '0' when others;
  with addr select we1 <= we_gate when 1, '0' when others;
  with addr select we2 <= we_gate when 2, '0' when others;
  with addr select we3 <= we_gate when 3, '0' when others;

  ---------------------------------------------------------------------------
  -- REG 0 — MODE bit
  ---------------------------------------------------------------------------
  reg_mode : entity work.vga_generic_register
    generic map (N => 1)
    port map (clk => clk, reset => rst, enable => we0,
              d   => mode_d, q => mode_q);

  mode_d(0) <= wdata(0);

  ---------------------------------------------------------------------------
  -- REG 1 — BUF bit
  ---------------------------------------------------------------------------
  reg_buf : entity work.vga_generic_register
    generic map (N => 1)
    port map (clk => clk, reset => rst, enable => we1,
              d   => buf_d, q => buf_q);

  buf_d(0) <= wdata(0);

  ---------------------------------------------------------------------------
  -- REG 2 — VSIEN bit
  ---------------------------------------------------------------------------
  reg_vsien : entity work.vga_generic_register
    generic map (N => 1)
    port map (clk => clk, reset => rst, enable => we2,
              d   => vsien_d, q => vsien_q);

  vsien_d(0) <= wdata(0);

  ---------------------------------------------------------------------------
  -- REG 3 — VSIA bit (R/W1C, hardware-set)
  -- Hardware (vsia_pending) always wins: the register mirrors vsia_pending
  -- every cycle (enable='1').  interrupt_controller owns the set/clear logic;
  -- software write generates a clear pulse but does not directly overwrite
  -- the stored bit — interrupt_controller drives vsia_pending low next cycle.
  ---------------------------------------------------------------------------
  reg_vsia : entity work.vga_generic_register
    generic map (N => 1)
    port map (clk => clk, reset => rst, enable => '1',
              d   => vsia_d, q => vsia_q);

  vsia_d(0) <= vsia_pending;

  ---------------------------------------------------------------------------
  -- Outputs
  ---------------------------------------------------------------------------
  -- Combinational read port: four-input mux; unimplemented bits [31:1] = 0.
  with addr select rdata <=
    (0 => mode_q(0),  others => '0') when 0,
    (0 => buf_q(0),   others => '0') when 1,
    (0 => vsien_q(0), others => '0') when 2,
    (0 => vsia_q(0),  others => '0') when others;

  mode       <= mode_q(0);
  buf_select <= buf_q(0);
  vsien      <= vsien_q(0);
  vsia_clear <= we3 and wdata(0);

end architecture rtl;
