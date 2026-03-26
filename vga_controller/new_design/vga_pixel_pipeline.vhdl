--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- VGA Pixel Pipeline                                         vga_pixel_pipeline
--
-- Self-contained display path from control registers to RGB output.
-- Instantiates vga_timing_gen (h/v counters, sync, video_on, fb address).
--
-- The framebuffer_bram and palette_ram instances live in the parent
-- (vga_controller) so that port-A (AXI write) and port-B (display read) are
-- accessible from a single structural level.  This entity receives the
-- port-B read results as input ports and drives port-B addresses as outputs.
--
-- Signal pipeline (two registered stages)
-- ----------------------------------------
--   vga_timing_gen generates fb_addr one cycle ahead of the pixel it
--   describes (the BRAM has a one-cycle output register).
--
--   Cycle 0: fb_addr driven to parent-owned BRAM port B
--   Cycle 1: fb_pixel_index (8-bit colour index) valid from BRAM output
--   Cycle 2: pal_pixel_rgb (12-bit RGB) valid from palette output,
--            gated by video_on to produce red/green/blue
--
--   hsync and vsync are registered inside vga_timing_gen (one clock behind
--   h_count / v_count) and are valid on the same cycle as their pixel.
--
-- Ports
-- -----
--   pixel_clk, reset, mode_320x200, buffer_select  control inputs
--   hsync, vsync, vsync_out, video_on              timing outputs
--   fb_addr, fb_read_en                            port-B address to parent BRAM
--   fb_pixel_index                                 port-B data from parent BRAM
--   pal_pixel_rgb                                  port-B data from parent palette
--   red, green, blue                               RGB output
--
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.vga_pkg.all;

entity vga_pixel_pipeline is
  port (
    pixel_clk     : in  std_logic;
    reset         : in  std_logic;
    mode_320x200  : in  std_logic;
    buffer_select : in  std_logic;

    hsync     : out std_logic;
    vsync     : out std_logic;
    vsync_out : out std_logic;
    video_on  : out std_logic;

    fb_addr    : out std_logic_vector(FB_ADDR_WIDTH-1 downto 0);
    fb_read_en : out std_logic;

    fb_pixel_index : in  std_logic_vector(7  downto 0);
    pal_pixel_rgb  : in  std_logic_vector(11 downto 0);

    red   : out std_logic_vector(3 downto 0);
    green : out std_logic_vector(3 downto 0);
    blue  : out std_logic_vector(3 downto 0)
  );
end entity vga_pixel_pipeline;

architecture rtl of vga_pixel_pipeline is

  signal vsync_i    : std_logic;
  signal video_on_i : std_logic;

begin

  ---------------------------------------------------------------------------
  -- Global internal signals
  ---------------------------------------------------------------------------
  -- (none beyond the sub-entity connections below)

  ---------------------------------------------------------------------------
  -- VGA timing generator
  ---------------------------------------------------------------------------
  timing_inst : entity work.vga_timing_gen
    port map (
      pixel_clk     => pixel_clk,
      reset         => reset,
      mode_320x200  => mode_320x200,
      buffer_select => buffer_select,
      hsync         => hsync,
      vsync         => vsync_i,
      video_on      => video_on_i,
      fb_addr       => fb_addr,
      fb_read_en    => fb_read_en
    );

  ---------------------------------------------------------------------------
  -- Outputs
  ---------------------------------------------------------------------------
  vsync    <= vsync_i;
  vsync_out <= vsync_i;
  video_on <= video_on_i;

  -- RGB blanking gate: force outputs to zero during blanking so monitors
  -- that ignore video_on see black rather than a stale palette colour.
  red   <= pal_pixel_rgb(11 downto 8) when video_on_i = '1' else (others => '0');
  green <= pal_pixel_rgb(7  downto 4) when video_on_i = '1' else (others => '0');
  blue  <= pal_pixel_rgb(3  downto 0) when video_on_i = '1' else (others => '0');

end architecture rtl;
