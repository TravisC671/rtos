--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- Generic Register Bank                                         generic_reg_bank
--
-- NUM_REGS x 32-bit software-accessible registers with a flat bus interface.
-- No application-specific port names; no interrupt logic.
--
-- Each register is stored as four independent 8-bit vga_generic_register
-- instances (one per byte lane) so that AXI WSTRB byte-enables are honoured
-- without a read-modify-write cycle.
--
-- Hardware writes (reg_hw_wr) take priority over software writes.
-- When reg_hw_wr(i)='1', all four byte lanes of register i are
-- unconditionally loaded from reg_hw_in regardless of we/wstrb/addr.
--
-- Interface
-- ---------
--   clk, resetn        clock and active-low reset
--   addr               word index (0 .. NUM_REGS-1)
--   we                 software write enable
--   wstrb              byte enables (4 bits for 32-bit data)
--   wdata              32-bit write data
--   rdata              32-bit combinational read data
--   reg_hw_in          hardware write data  (t_reg_array)
--   reg_hw_wr          per-register hardware write enable vector
--   reg_out            current register values (t_reg_array)
--
-- Design rules: no process statements; all concurrent signal assignments.
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.vga_pkg.all;

entity generic_reg_bank is
  generic (
    NUM_REGS   : integer := 4;
    DATA_WIDTH : integer := 32
  );
  port (
    clk    : in  std_logic;
    resetn : in  std_logic;

    addr   : in  integer range 0 to NUM_REGS-1;
    we     : in  std_logic;
    wstrb  : in  std_logic_vector(DATA_WIDTH/8-1 downto 0);
    wdata  : in  std_logic_vector(DATA_WIDTH-1   downto 0);
    rdata  : out std_logic_vector(DATA_WIDTH-1   downto 0);

    reg_hw_in : in  t_reg_array(0 to NUM_REGS-1);
    reg_hw_wr : in  std_logic_vector(NUM_REGS-1 downto 0);
    reg_out   : out t_reg_array(0 to NUM_REGS-1)
  );
end entity generic_reg_bank;

architecture rtl of generic_reg_bank is

  constant BYTES : integer := DATA_WIDTH / 8;

  signal rst : std_logic;

  -- addr_hit(i) = '1' when the current software access targets register i
  type hit_t is array(0 to NUM_REGS-1) of std_logic;
  signal addr_hit : hit_t;

  -- Per-register assembled storage and pre-computed enable/data
  type reg_data_t  is array(0 to NUM_REGS-1) of std_logic_vector(DATA_WIDTH-1 downto 0);
  type byte_en_t   is array(0 to NUM_REGS-1) of std_logic_vector(BYTES-1 downto 0);

  signal byte_q  : reg_data_t;
  signal byte_en : byte_en_t;
  signal byte_d  : reg_data_t;

begin

  ---------------------------------------------------------------------------
  -- Global internal signals
  ---------------------------------------------------------------------------
  rst <= not resetn;

  -- Address hit decode (one-hot)
  gen_hit : for i in 0 to NUM_REGS-1 generate
    addr_hit(i) <= '1' when addr = i else '0';
  end generate gen_hit;

  ---------------------------------------------------------------------------
  -- Per-register byte-lane registers
  -- For each register i and byte lane b:
  --   byte_d(i)   data mux: hardware data when hw write, else software data
  --   byte_en(i)  enable:   hardware write always wins; SW needs we+wstrb+hit
  --   byte_q(i)   assembled output
  ---------------------------------------------------------------------------
  gen_regs : for i in 0 to NUM_REGS-1 generate

    gen_bytes : for b in 0 to BYTES-1 generate

      byte_reg_inst : entity work.vga_generic_register
        generic map (N => 8)
        port map (
          clk    => clk,
          reset  => rst,
          enable => byte_en(i)(b),
          d      => byte_d(i)((b+1)*8-1 downto b*8),
          q      => byte_q(i)((b+1)*8-1 downto b*8)
        );

      byte_en(i)(b) <= reg_hw_wr(i) or (we and wstrb(b) and addr_hit(i));

    end generate gen_bytes;

    byte_d(i)  <= reg_hw_in(i) when reg_hw_wr(i) = '1' else wdata;
    reg_out(i) <= byte_q(i);

  end generate gen_regs;

  ---------------------------------------------------------------------------
  -- Outputs
  ---------------------------------------------------------------------------
  rdata <= byte_q(addr);

end architecture rtl;
