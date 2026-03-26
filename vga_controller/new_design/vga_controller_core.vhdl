--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- VGA Controller Core                                          vga_controller_core
--
-- Top-level structural core.  All logic lives in sub-entities.
-- Instantiated by vga_controller, which provides a flat std_logic port wrapper
-- for Vivado IP packager compatibility.
--
-- Hierarchy
-- ---------
--   axi_peripheral_bus   AXI4 FSMs + datapaths + address decode + arbiter
--   register_bank        4 named VGA control/status registers
--   interrupt_controller VSYNC edge-detect, W1C pending flag, IRQ output
--   framebuffer_bram     256 KB dual-port pixel BRAM
--   palette_ram          256-entry 12-bit colour LUT (distributed RAM)
--   vga_pixel_pipeline   vga_timing_gen + RGB blanking gate
--
-- The BRAM and palette live here (rather than inside vga_pixel_pipeline)
-- so that port A (AXI write) and port B (VGA read) are both accessible from
-- a single structural level.  axi_peripheral_bus drives port-A signals;
-- vga_pixel_pipeline drives port-B read addresses and receives pixel data.
--
-- AXI address map (byte addresses)
--   0x00000 .. 0x3FFFF  framebuffer_bram   (256 KB)
--   0x40000 .. 0x401FF  palette_ram        (512 B, 256 x 12-bit entries)
--   0x40200             Control Register      (MODE bit)
--   0x40204             Display Buffer Reg    (BUF bit)
--   0x40208             IRQ Enable Register   (VSIEN)
--   0x4020C             IRQ Clear/Status Reg  (VSIA W1C)
--
-- AXI interface
-- -------------
-- Uses vga_pkg record types:
--   s_axi_in  : in  t_axi4_m2s   master-to-slave signals
--   s_axi_out : out t_axi4_s2m   slave-to-master signals
--
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.vga_pkg.all;

entity vga_controller_core is
  generic (
    AXI_ADDR_WIDTH : integer := 32;
    AXI_DATA_WIDTH : integer := 32;
    AXI_ID_WIDTH   : integer := 4
  );
  port (
    axi_clk      : in  std_logic;
    aresetn   : in  std_logic;
    pixel_clk : in  std_logic;

    -- AXI4 slave interface (record types from vga_pkg)
    s_axi_in  : in  t_axi4_m2s;
    s_axi_out : out t_axi4_s2m;

    hsync    : out std_logic;
    vsync    : out std_logic;
    video_on : out std_logic;
    red, green, blue : out std_logic_vector(3 downto 0);
    irq      : out std_logic
  );
end entity vga_controller_core;

architecture rtl of vga_controller_core is

  constant DATA_BYTES : natural := AXI_DATA_WIDTH / 8;
  signal rst : std_logic;

  ---------------------------------------------------------------------------
  -- Peripheral bus -> BRAM / palette / register bank
  ---------------------------------------------------------------------------
  signal bram_en_a   : std_logic;
  signal bram_we_a   : std_logic;
  signal bram_addr_a : std_logic_vector(FB_ADDR_WIDTH-1 downto 0);
  signal bram_din_a  : std_logic_vector(AXI_DATA_WIDTH-1 downto 0);
  signal bram_dout_a : std_logic_vector(AXI_DATA_WIDTH-1 downto 0);

  signal pal_en_a   : std_logic;
  signal pal_we_a   : std_logic;
  signal pal_addr_a : std_logic_vector(6 downto 0);
  signal pal_din_a  : std_logic_vector(AXI_DATA_WIDTH-1 downto 0);
  signal pal_dout_a : std_logic_vector(AXI_DATA_WIDTH-1 downto 0);

  signal reg_addr  : integer range 0 to 3;
  signal reg_we    : std_logic;
  signal reg_wstrb : std_logic_vector(DATA_BYTES-1 downto 0);
  signal reg_wdata : std_logic_vector(AXI_DATA_WIDTH-1 downto 0);
  signal reg_rdata : std_logic_vector(31 downto 0);

  ---------------------------------------------------------------------------
  -- Register bank outputs
  ---------------------------------------------------------------------------
  signal reg_mode     : std_logic;
  signal reg_buf      : std_logic;
  signal reg_vsien    : std_logic;
  signal reg_vsia_clr : std_logic;

  ---------------------------------------------------------------------------
  -- Interrupt controller
  ---------------------------------------------------------------------------
  signal irq_pending : std_logic;
  signal vsync_i     : std_logic;

  ---------------------------------------------------------------------------
  -- Pixel pipeline <-> BRAM / palette port B
  ---------------------------------------------------------------------------
  signal fb_addr_b     : std_logic_vector(FB_ADDR_WIDTH-1 downto 0);
  signal fb_read_en_b  : std_logic;
  signal fb_pixel_index : std_logic_vector(7 downto 0);
  signal pal_pixel_rgb  : std_logic_vector(11 downto 0);

begin

  ---------------------------------------------------------------------------
  -- Global internal signals
  ---------------------------------------------------------------------------
  rst <= not aresetn;

  ---------------------------------------------------------------------------
  -- AXI peripheral bus (protocol engine + address decoder)
  ---------------------------------------------------------------------------
  bus_inst : entity work.axi_peripheral_bus
    generic map (
      ADDR_WIDTH => AXI_ADDR_WIDTH,
      DATA_WIDTH => AXI_DATA_WIDTH,
      ID_WIDTH   => AXI_ID_WIDTH
    )
    port map (
      axi_clk        => axi_clk,        aresetn     => aresetn,
      s_axi_in    => s_axi_in,    s_axi_out   => s_axi_out,
      bram_en_a   => bram_en_a,   bram_we_a   => bram_we_a,
      bram_addr_a => bram_addr_a, bram_din_a  => bram_din_a,
      bram_dout_a => bram_dout_a,
      pal_en_a    => pal_en_a,    pal_we_a    => pal_we_a,
      pal_addr_a  => pal_addr_a,  pal_din_a   => pal_din_a,
      pal_dout_a  => pal_dout_a,
      reg_addr    => reg_addr,    reg_we      => reg_we,
      reg_wstrb   => reg_wstrb,   reg_wdata   => reg_wdata,
      reg_rdata   => reg_rdata
    );

  ---------------------------------------------------------------------------
  -- Register bank
  ---------------------------------------------------------------------------
  regbank_inst : entity work.register_bank
    port map (
      clk          => axi_clk,         resetn       => aresetn,
      addr         => reg_addr,     we           => reg_we,
      wstrb        => reg_wstrb,    wdata        => reg_wdata,
      rdata        => reg_rdata,
      mode         => reg_mode,     buf_select   => reg_buf,
      vsien        => reg_vsien,
      vsia_clear   => reg_vsia_clr, vsia_pending => irq_pending
    );

  ---------------------------------------------------------------------------
  -- Interrupt controller
  ---------------------------------------------------------------------------
  irq_ctrl_inst : entity work.interrupt_controller
    port map (
      clk     => axi_clk,          resetn  => aresetn,
      event   => vsync_i,
      enable  => reg_vsien,     clear   => reg_vsia_clr,
      pending => irq_pending,   irq     => irq
    );

  ---------------------------------------------------------------------------
  -- Framebuffer BRAM
  -- Port A: AXI write path (from axi_peripheral_bus)
  -- Port B: VGA read path  (from vga_pixel_pipeline)
  ---------------------------------------------------------------------------
  bram_inst : entity work.framebuffer_bram
    generic map (ADDR_WIDTH => FB_ADDR_WIDTH, DATA_WIDTH => AXI_DATA_WIDTH)
    port map (
      clk_a  => axi_clk,        en_a   => bram_en_a,
      we_a   => bram_we_a,   addr_a => bram_addr_a,
      din_a  => bram_din_a,  dout_a => bram_dout_a,
      clk_b  => pixel_clk,   en_b   => fb_read_en_b,
      addr_b => fb_addr_b,   dout_b => fb_pixel_index
    );

  ---------------------------------------------------------------------------
  -- Palette RAM
  -- Port A: AXI write path (from axi_peripheral_bus)
  -- Port B: VGA colour lookup (pixel_index -> RGB)
  ---------------------------------------------------------------------------
  palette_inst : entity work.palette_ram
    generic map (AXI_ADDR_WIDTH => 7, PIXEL_ADDR_WIDTH => 8,
                 DATA_WIDTH => AXI_DATA_WIDTH)
    port map (
      clk_a  => axi_clk,           en_a   => pal_en_a,
      we_a   => pal_we_a,       addr_a => pal_addr_a,
      din_a  => pal_din_a,      dout_a => pal_dout_a,
      clk_b  => pixel_clk,      en_b   => '1',
      addr_b => fb_pixel_index, dout_b => pal_pixel_rgb
    );

  ---------------------------------------------------------------------------
  -- VGA pixel pipeline
  ---------------------------------------------------------------------------
  pipeline_inst : entity work.vga_pixel_pipeline
    port map (
      pixel_clk      => pixel_clk,   reset         => rst,
      mode_320x200   => reg_mode,    buffer_select => reg_buf,
      hsync          => hsync,       vsync         => vsync,
      vsync_out      => vsync_i,     video_on      => video_on,
      fb_addr        => fb_addr_b,   fb_read_en    => fb_read_en_b,
      fb_pixel_index => fb_pixel_index,
      pal_pixel_rgb  => pal_pixel_rgb,
      red            => red,         green         => green,
      blue           => blue
    );

end architecture rtl;
