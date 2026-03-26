--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- VGA Timing Generator                                           vga_timing_gen
--
-- Generates 640x400 @ 70 Hz VGA timing with dual resolution support.
--   Mode 0 (mode_320x200='1'): 320x200 framebuffer doubled to 640x400 (2x2)
--   Mode 1 (mode_320x200='0'): 640x400 framebuffer mapped 1:1
--
-- Hierarchy
-- ---------
--   vga_hv_counters   h/v raster scan counters  (vga_hv_counters.vhdl)
--   vga_timing_gen    sync, video_on, framebuffer address  (this file)
--
-- Pipeline note
-- -------------
-- The framebuffer BRAM has a one-cycle registered output, so fb_addr must be
-- presented ONE cycle ahead of the pixel that will appear on screen.
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.vga_pkg.all;

entity vga_timing_gen is
  port (
    pixel_clk     : in  std_logic;
    reset         : in  std_logic;
    mode_320x200  : in  std_logic;
    buffer_select : in  std_logic;
    hsync         : out std_logic;
    vsync         : out std_logic;
    video_on      : out std_logic;
    fb_addr       : out std_logic_vector(FB_ADDR_WIDTH-1 downto 0);
    fb_read_en    : out std_logic
  );
end entity vga_timing_gen;

architecture rtl of vga_timing_gen is

  -- Counter outputs from vga_hv_counters
  signal h_count      : unsigned(9 downto 0);
  signal v_count      : unsigned(9 downto 0);
  signal h_at_end     : std_logic;
  signal h_count_next : unsigned(9 downto 0);
  signal v_count_next : unsigned(9 downto 0);

  -- Sync and video-on registers (1-bit, wrapped in slv(0 downto 0))
  signal hsync_next    : std_logic;
  signal hsync_d       : std_logic_vector(0 downto 0);
  signal hsync_q       : std_logic_vector(0 downto 0);

  signal vsync_next    : std_logic;
  signal vsync_d       : std_logic_vector(0 downto 0);
  signal vsync_q       : std_logic_vector(0 downto 0);

  signal video_on_next : std_logic;
  signal video_on_d    : std_logic_vector(0 downto 0);
  signal video_on_q    : std_logic_vector(0 downto 0);

  -- Framebuffer address calculation
  signal fb_x             : unsigned(9 downto 0);
  signal fb_y             : unsigned(9 downto 0);
  signal fb_addr_int      : unsigned(FB_ADDR_WIDTH-1 downto 0);
  signal in_active_region : std_logic;
  signal buffer_offset    : unsigned(FB_ADDR_WIDTH-1 downto 0);

  -- Mode-selector aliases
  -- fb_mode_sel: 2-bit selector for fb_x / fb_y with/select muxes
  --   "10"  320x200 active    "01"  640x400 active    "00"  blanking
  signal mode_low_active  : std_logic;
  signal mode_high_active : std_logic;
  signal fb_mode_sel      : std_logic_vector(1 downto 0);

begin

  ---------------------------------------------------------------------------
  -- Global internal signals
  ---------------------------------------------------------------------------
  mode_low_active  <=     mode_320x200 and in_active_region;
  mode_high_active <= not mode_320x200 and in_active_region;
  fb_mode_sel      <= mode_low_active & mode_high_active;

  in_active_region <= '1' when h_count_next < H_DISPLAY and
                               v_count_next < V_DISPLAY else '0';

  buffer_offset <= to_unsigned(FB_BUFFER_1_BASE, FB_ADDR_WIDTH)
                   when buffer_select = '1' else
                   to_unsigned(0, FB_ADDR_WIDTH);

  with fb_mode_sel select fb_x <=
    resize(h_count_next(9 downto 1), 10) when "10",
    h_count_next                          when "01",
    (others => '0')                       when others;

  with fb_mode_sel select fb_y <=
    resize(v_count_next(8 downto 1), 10) when "10",
    v_count_next                          when "01",
    (others => '0')                       when others;

  -- Address arithmetic:
  --   320x200: y*320 + x = (y<<8)+(y<<6)+x  + buffer_offset
  --   640x400: y*640 + x = (y<<9)+(y<<7)+x
  fb_addr_int <=
    (resize(fb_y, FB_ADDR_WIDTH) sll 8) +
    (resize(fb_y, FB_ADDR_WIDTH) sll 6) +
     resize(fb_x, FB_ADDR_WIDTH)        +
     buffer_offset
    when mode_320x200 = '1' else
    (resize(fb_y, FB_ADDR_WIDTH) sll 9) +
    (resize(fb_y, FB_ADDR_WIDTH) sll 7) +
     resize(fb_x, FB_ADDR_WIDTH);

  ---------------------------------------------------------------------------
  -- H/V counter FSM
  -- h_count_next and v_count_next come directly from the sub-entity;
  -- no look-ahead logic is replicated here.
  ---------------------------------------------------------------------------
  hv_counters_inst : entity work.vga_hv_counters
    port map (
      pixel_clk    => pixel_clk,
      reset        => reset,
      h_count      => h_count,
      v_count      => v_count,
      h_at_end     => h_at_end,
      h_count_next => h_count_next,
      v_count_next => v_count_next
    );

  ---------------------------------------------------------------------------
  -- Horizontal sync register
  ---------------------------------------------------------------------------
  reg_hsync : entity work.vga_generic_register
    generic map (N => 1)
    port map (clk => pixel_clk, reset => reset, enable => '1',
              d   => hsync_d,   q     => hsync_q);

  hsync_next <= H_SYNC_POL when h_count >= (H_DISPLAY + H_FRONT_PORCH) and
                                h_count <  (H_DISPLAY + H_FRONT_PORCH + H_SYNC_PULSE) else
               not H_SYNC_POL;
  hsync_d(0) <= hsync_next;

  ---------------------------------------------------------------------------
  -- Vertical sync register
  ---------------------------------------------------------------------------
  reg_vsync : entity work.vga_generic_register
    generic map (N => 1)
    port map (clk => pixel_clk, reset => reset, enable => '1',
              d   => vsync_d,   q     => vsync_q);

  vsync_next <= V_SYNC_POL when v_count >= (V_DISPLAY + V_FRONT_PORCH) and
                                v_count <  (V_DISPLAY + V_FRONT_PORCH + V_SYNC_PULSE) else
               not V_SYNC_POL;
  vsync_d(0) <= vsync_next;

  ---------------------------------------------------------------------------
  -- Video-on register
  ---------------------------------------------------------------------------
  reg_video_on : entity work.vga_generic_register
    generic map (N => 1)
    port map (clk => pixel_clk, reset => reset, enable => '1',
              d   => video_on_d, q   => video_on_q);

  video_on_next <= '1' when h_count < H_DISPLAY and v_count < V_DISPLAY else
                   '0';
  video_on_d(0) <= video_on_next;

  ---------------------------------------------------------------------------
  -- Outputs
  ---------------------------------------------------------------------------
  hsync      <= hsync_q(0);
  vsync      <= vsync_q(0);
  video_on   <= video_on_q(0);
  fb_addr    <= std_logic_vector(fb_addr_int);
  fb_read_en <= in_active_region;

end architecture rtl;
