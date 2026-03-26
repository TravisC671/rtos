--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- Color Palette RAM
-- Dual-port RAM for 256 x 12-bit palette entries.
-- Physically stored as 128 x 32-bit words; each word holds two 16-bit
-- halfwords (upper 4 bits of each halfword unused, lower 12 bits = RGB):
--   bits [15:0]  even entry (colour index 2n)
--   bits [31:16] odd  entry (colour index 2n+1)
-- Port A: AXI/CPU interface (read/write, 32-bit word access, 7-bit word addr)
-- Port B: VGA lookup interface (read-only, 8-bit colour index, 12-bit RGB out)
--
-- Implemented as distributed (LUT) RAM rather than BRAM.  At 128x32 bits
-- (4,096 bits) the palette occupies only 11% of one BRAM36; distributed RAM
-- frees that primitive at a cost of ~64-100 LUT6s (<0.2% of XC7A100T LUTs).
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.vga_pkg.all;

entity palette_ram is
  generic(
    AXI_ADDR_WIDTH   : integer := 7; -- bits 9 downto 2 of the axi address
    PIXEL_ADDR_WIDTH : integer := 8; -- index to the halfword with the rgb values
    DATA_WIDTH : integer := 32);

  Port (
    -- Port A: AXI/CPU interface
    clk_a       : in  std_logic;
    en_a        : in  std_logic;
    we_a        : in  std_logic;
    addr_a      : in  std_logic_vector(AXI_ADDR_WIDTH-1 downto 0);
    din_a       : in  std_logic_vector(31 downto 0);
    dout_a      : out std_logic_vector(31 downto 0);
    
    -- Port B: VGA read interface
    clk_b       : in  std_logic;
    en_b        : in  std_logic;
    addr_b      : in  std_logic_vector(PIXEL_ADDR_WIDTH-1 downto 0);
    dout_b      : out std_logic_vector(11 downto 0)   -- 12-bit RGB output
    );
end palette_ram;

architecture Behavioral of palette_ram is
  -- Palette array - 128 x 32 bits (256 entries packed two per word)
  type ram_type is array (0 to (2**AXI_ADDR_WIDTH)-1) of std_logic_vector(31 downto 0);
  shared variable RAM : ram_type := (others => (others => '0'));
  
  -- Synthesise as distributed (LUT) RAM rather than BRAM.
  -- The palette is only 128x32 bits (4,096 bits = 11% of one BRAM36); using
  -- a full BRAM36 primitive wastes the remaining 89%.  Distributed RAM costs
  -- ~64-100 LUT6s and 32 FFs -- less than 0.2% of available LUTs on an
  -- XC7A100T -- and frees one BRAM36 for other use.
  --
  -- CDC note: Port A (AXI clk_a) writes; Port B (VGA clk_b) reads every pixel.
  -- This is a cross-clock-domain access, but benign: the palette is written
  -- once at initialisation and is effectively read-only during operation.
  -- A glitch on a single pixel during a mid-operation palette write is
  -- acceptable.  If runtime palette updates are ever required, Grey-code the
  -- address or add a vsync-aligned write window.
  attribute ram_style : string;
  attribute ram_style of RAM : variable is "distributed";

  signal dout_b_word : std_logic_vector(31 downto 0);
  signal word_addr_b  : std_logic_vector(AXI_ADDR_WIDTH-1 downto 0);    -- 7 bits: colour index >> 1
  signal addr_b_lsb_q : std_logic;                                           -- registered addr_b(0) for even/odd mux
  
begin

  word_addr_b <= addr_b(PIXEL_ADDR_WIDTH-1 downto 1);  -- shift right by 1: colour index -> word address
  
  -- Port A process (read/write)
  -- Upper 4 bits of each 16-bit halfword are masked to zero on write (RAZ/WI).
  -- This ensures readback never returns garbage in the unused nibbles.
  process(clk_a)
  begin
    if rising_edge(clk_a) then
      if en_a = '1' then
        if we_a = '1' then
          RAM(to_integer(unsigned(addr_a))) :=
            "0000" & din_a(27 downto 16) &   -- odd  entry: mask bits[31:28]
            "0000" & din_a(11 downto 0);     -- even entry: mask bits[15:12]
        end if;
        dout_a <= RAM(to_integer(unsigned(addr_a)));
      end if;
    end if;
  end process;
  
  -- Port B process (read-only)
  -- addr_b(0) is registered alongside the BRAM word so the even/odd mux
  -- uses the address that was valid when the read was initiated.
  process(clk_b)
  begin
    if rising_edge(clk_b) then
      if en_b = '1' then
        dout_b_word  <= RAM(to_integer(unsigned(word_addr_b)));
        addr_b_lsb_q <= addr_b(0);
      end if;
    end if;
  end process;

  dout_b <=
    dout_b_word(11 downto 0) when addr_b_lsb_q = '0' else
    dout_b_word(27 downto 16);
  

end Behavioral;
