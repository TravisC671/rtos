--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- Dual-Port Framebuffer BRAM
-- Port A: AXI4 slave interface (read/write)
-- Port B: VGA read interface (read-only)
-- Size: 256,000 bytes (64,000 x 32-bit words)
-- Supports 320x200 double-buffered or 640x400 single-buffered
--
-- Array depth is set by FB_BRAM_DEPTH from vga_pkg rather than
-- rounding up to the next power of two.  640x400 = 256,000 bytes = 64,000
-- words; the next power of two would be 65,536 words = 262,144 bytes.
-- The exact depth correctly documents intent and eliminates phantom addresses,
-- but does not reduce the BRAM36 primitive count: Vivado still infers 64
-- RAMB36 because 64,000 is not a power of two and each tile must be
-- power-of-2 deep, forcing an extra tile for address coverage.
-- The BRAM36 count reduction (65->64) came from moving the palette to
-- distributed (LUT) RAM in palette_bram.vhdl.
--
-- Byte-lane mux timing
-- --------------------
-- The BRAM has a one-cycle registered output: dout_b_word reflects the word
-- at word_addr_b from the PREVIOUS clock cycle.  The byte-lane selector
-- addr_b(1 downto 0) must therefore also be registered by one cycle so it
-- stays aligned with the word it is selecting from.  Using the live
-- addr_b(1 downto 0) directly produces a sawtooth scramble at every
-- 4-pixel word boundary because the selector is one cycle ahead of the data.
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.vga_pkg.all;

entity framebuffer_bram is
  generic(
    ADDR_WIDTH : integer := 18;
    DATA_WIDTH : integer := 32);

  Port (
    -- Port A: AXI/CPU interface
    clk_a       : in  std_logic;
    en_a        : in  std_logic;
    we_a        : in  std_logic;
    addr_a      : in  std_logic_vector(ADDR_WIDTH-1 downto 0);
    din_a       : in  std_logic_vector(31 downto 0);
    dout_a      : out std_logic_vector(31 downto 0);
    
    -- Port B: VGA read interface
    clk_b       : in  std_logic;
    en_b        : in  std_logic;
    addr_b      : in  std_logic_vector(FB_ADDR_WIDTH-1 downto 0);
    dout_b      : out std_logic_vector(7 downto 0)
    );
end framebuffer_bram;

architecture Behavioral of framebuffer_bram is
  type ram_type is array (0 to FB_BRAM_DEPTH-1) of std_logic_vector(31 downto 0);
  shared variable RAM : ram_type := (others => (others => '0'));
  
  attribute ram_style : string;
  attribute ram_style of RAM : variable is "block";

  signal dout_b_word   : std_logic_vector(31 downto 0);
  signal byte_sel_q    : std_logic_vector(1 downto 0);  -- registered addr_b(1:0)
  signal word_addr_a   : std_logic_vector(ADDR_WIDTH-3 downto 0);
  signal word_addr_b   : std_logic_vector(ADDR_WIDTH-3 downto 0);
  
begin
  word_addr_a <= addr_a(ADDR_WIDTH-1 downto 2);
  word_addr_b <= addr_b(ADDR_WIDTH-1 downto 2);
  
  -- Port A process (read/write)
  process(clk_a)
  begin
    if rising_edge(clk_a) then
      if en_a = '1' then
        if we_a = '1' then
          RAM(to_integer(unsigned(word_addr_a))) := din_a;
        end if;
        dout_a <= RAM(to_integer(unsigned(word_addr_a)));
      end if;
    end if;
  end process;
  
  -- Port B process (read-only).
  -- byte_sel_q is registered in the same process as dout_b_word so both
  -- are delayed by exactly one cycle relative to addr_b.  The mux below
  -- therefore always selects from the correct word.
  process(clk_b)
  begin
    if rising_edge(clk_b) then
      if en_b = '1' then
        dout_b_word <= RAM(to_integer(unsigned(word_addr_b)));
        byte_sel_q  <= addr_b(1 downto 0);
      end if;
    end if;
  end process;

  with byte_sel_q select
    dout_b <=
      dout_b_word(7  downto  0) when "00",
      dout_b_word(15 downto  8) when "01",
      dout_b_word(23 downto 16) when "10",
      dout_b_word(31 downto 24) when others;
  
end Behavioral;
