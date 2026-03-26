--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- VGA Controller                                                  vga_controller
--
-- Thin wrapper around vga_controller_core that exposes all AXI4 signals as flat
-- std_logic / std_logic_vector ports instead of records.  Required for Vivado
-- IP packager, which does not accept user-defined record types at the top level.
--
-- All logic lives in vga_controller_core; this entity only packs/unpacks the
-- t_axi4_m2s and t_axi4_s2m records.
--
-- Ports
-- -----
--   axi_clk, pixel_clk   clocks
--   aresetn               AXI active-low synchronous reset
--   s_axi_aw*             AXI write-address channel  (master -> slave)
--   s_axi_w*              AXI write-data channel     (master -> slave)
--   s_axi_b*              AXI write-response channel (slave  -> master)
--   s_axi_ar*             AXI read-address channel   (master -> slave)
--   s_axi_r*              AXI read-data channel      (slave  -> master)
--   hsync, vsync          VGA sync outputs
--   red, green, blue      4-bit RGB pixel outputs
--   video_on              active-display flag
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use work.vga_pkg.all;

entity vga_controller is
 generic (
    AXI_ADDR_WIDTH : integer := 32;
    AXI_DATA_WIDTH : integer := 32;
    AXI_ID_WIDTH   : integer := 4
  );
  port (
    -- Clocks and reset
    axi_clk       : in  std_logic;
    pixel_clk  : in  std_logic;
    aresetn    : in  std_logic;

    -- AXI write-address channel (master -> slave)
    s_axi_awid    : in  std_logic_vector(3  downto 0);
    s_axi_awaddr  : in  std_logic_vector(31 downto 0);
    s_axi_awlen   : in  std_logic_vector(7  downto 0);
    s_axi_awsize  : in  std_logic_vector(2  downto 0);
    s_axi_awburst : in  std_logic_vector(1  downto 0);
    s_axi_awvalid : in  std_logic;
    s_axi_awready : out std_logic;

    -- AXI write-data channel (master -> slave)
    s_axi_wdata   : in  std_logic_vector(31 downto 0);
    s_axi_wstrb   : in  std_logic_vector(3  downto 0);
    s_axi_wvalid  : in  std_logic;
    s_axi_wready  : out std_logic;

    -- AXI write-response channel (slave -> master)
    s_axi_bid     : out std_logic_vector(3  downto 0);
    s_axi_bresp   : out std_logic_vector(1  downto 0);
    s_axi_bvalid  : out std_logic;
    s_axi_bready  : in  std_logic;

    -- AXI read-address channel (master -> slave)
    s_axi_arid    : in  std_logic_vector(3  downto 0);
    s_axi_araddr  : in  std_logic_vector(31 downto 0);
    s_axi_arlen   : in  std_logic_vector(7  downto 0);
    s_axi_arsize  : in  std_logic_vector(2  downto 0);
    s_axi_arburst : in  std_logic_vector(1  downto 0);
    s_axi_arvalid : in  std_logic;
    s_axi_arready : out std_logic;

    -- AXI read-data channel (slave -> master)
    s_axi_rid     : out std_logic_vector(3  downto 0);
    s_axi_rdata   : out std_logic_vector(31 downto 0);
    s_axi_rresp   : out std_logic_vector(1  downto 0);
    s_axi_rlast   : out std_logic;
    s_axi_rvalid  : out std_logic;
    s_axi_rready  : in  std_logic;

    -- VGA outputs
    hsync     : out std_logic;
    vsync     : out std_logic;
    red       : out std_logic_vector(3 downto 0);
    green     : out std_logic_vector(3 downto 0);
    blue      : out std_logic_vector(3 downto 0);
    video_on  : out std_logic;
    irq       : out std_logic
  );
end entity vga_controller;

architecture rtl of vga_controller is

  signal axi_in  : t_axi4_m2s;
  signal axi_out : t_axi4_s2m;

begin

  ---------------------------------------------------------------------------
  -- Pack flat input ports into t_axi4_m2s record
  ---------------------------------------------------------------------------
  axi_in.awid    <= s_axi_awid;
  axi_in.awaddr  <= s_axi_awaddr;
  axi_in.awlen   <= s_axi_awlen;
  axi_in.awsize  <= s_axi_awsize;
  axi_in.awburst <= s_axi_awburst;
  axi_in.awvalid <= s_axi_awvalid;
  axi_in.wdata   <= s_axi_wdata;
  axi_in.wstrb   <= s_axi_wstrb;
  axi_in.wvalid  <= s_axi_wvalid;
  axi_in.bready  <= s_axi_bready;
  axi_in.arid    <= s_axi_arid;
  axi_in.araddr  <= s_axi_araddr;
  axi_in.arlen   <= s_axi_arlen;
  axi_in.arsize  <= s_axi_arsize;
  axi_in.arburst <= s_axi_arburst;
  axi_in.arvalid <= s_axi_arvalid;
  axi_in.rready  <= s_axi_rready;

  ---------------------------------------------------------------------------
  -- Unpack t_axi4_s2m record into flat output ports
  ---------------------------------------------------------------------------
  s_axi_awready <= axi_out.awready;
  s_axi_wready  <= axi_out.wready;
  s_axi_bid     <= axi_out.bid;
  s_axi_bresp   <= axi_out.bresp;
  s_axi_bvalid  <= axi_out.bvalid;
  s_axi_arready <= axi_out.arready;
  s_axi_rid     <= axi_out.rid;
  s_axi_rdata   <= axi_out.rdata;
  s_axi_rresp   <= axi_out.rresp;
  s_axi_rlast   <= axi_out.rlast;
  s_axi_rvalid  <= axi_out.rvalid;

  ---------------------------------------------------------------------------
  -- vga_controller_core instantiation
  ---------------------------------------------------------------------------
  u_vga_controller : entity work.vga_controller_core
    port map (
      axi_clk       => axi_clk,
      pixel_clk  => pixel_clk,
      aresetn    => aresetn,
      s_axi_in   => axi_in,
      s_axi_out  => axi_out,
      hsync      => hsync,
      vsync      => vsync,
      red        => red,
      green      => green,
      blue       => blue,
      video_on   => video_on,
      irq        => irq
    );

end architecture rtl;
