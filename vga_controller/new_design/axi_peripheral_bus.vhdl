--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- AXI Peripheral Bus                                         axi_peripheral_bus
--
-- Structural wrapper that combines axi_slave (AXI4 protocol engine) and
-- axi_mem_arbiter (address decode + port-A steering) into a single entity.
--
-- Both vga_controller and axi_framebuffer previously instantiated these two
-- entities independently, wiring them together with identical signal sets.
-- This entity eliminates that duplication: each parent now makes a single
-- instantiation of axi_peripheral_bus and receives the decoded port-A control
-- signals and register-bank access signals directly.
--
-- AXI address map (byte addresses, as decoded by axi_mem_arbiter)
-- ---------------------------------------------------------------
--   0x00000 .. 0x3FFFF  framebuffer BRAM  (256 KB, byte addresses)
--   0x40000 .. 0x401FF  palette RAM       (512 B)
--   0x40200 .. 0x403FF  register bank     (4 x 32-bit words)
--
-- Ports
-- -----
--   axi_clk, aresetn          AXI clock and active-low reset
--   s_axi_in / s_axi_out   AXI4 slave interface (vga_pkg record types)
--
--   -- Framebuffer BRAM port-A control (to framebuffer_bram)
--   bram_en_a, bram_we_a, bram_addr_a, bram_din_a
--   bram_dout_a   (from framebuffer_bram port A back to the read-data mux)
--
--   -- Palette RAM port-A control (to palette_ram)
--   pal_en_a, pal_we_a, pal_addr_a, pal_din_a
--   pal_dout_a    (from palette_ram port A back to the read-data mux)
--
--   -- Register bank access
--   reg_addr, reg_we, reg_wstrb, reg_wdata, reg_rdata
--
-- Hierarchy (compile order)
-- -------------------------
--   vga_pkg.vhdl           types and constants
--   axi_channel_fsm.vhdl   axi_write_fsm, axi_read_fsm
--   axi_datapath.vhdl      axi_write_datapath, axi_read_datapath
--   axi_slave.vhdl         axi_slave
--   axi_mem_arbiter.vhdl   axi_mem_arbiter
--   axi_peripheral_bus.vhdl  (this file)
--
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.vga_pkg.all;

entity axi_peripheral_bus is
  generic (
    ADDR_WIDTH : integer := 32;
    DATA_WIDTH : integer := 32;
    ID_WIDTH   : integer := 4
  );
  port (
    axi_clk    : in  std_logic;
    aresetn : in  std_logic;

    -- AXI4 slave interface (record types from vga_pkg)
    s_axi_in  : in  t_axi4_m2s;
    s_axi_out : out t_axi4_s2m;

    ---------------------------------------------------------------------------
    -- Framebuffer BRAM port-A control
    ---------------------------------------------------------------------------
    bram_en_a   : out std_logic;
    bram_we_a   : out std_logic;
    bram_addr_a : out std_logic_vector(FB_ADDR_WIDTH-1 downto 0);
    bram_din_a  : out std_logic_vector(DATA_WIDTH-1 downto 0);
    bram_dout_a : in  std_logic_vector(DATA_WIDTH-1 downto 0);

    ---------------------------------------------------------------------------
    -- Palette RAM port-A control
    ---------------------------------------------------------------------------
    pal_en_a   : out std_logic;
    pal_we_a   : out std_logic;
    pal_addr_a : out std_logic_vector(6 downto 0);
    pal_din_a  : out std_logic_vector(DATA_WIDTH-1 downto 0);
    pal_dout_a : in  std_logic_vector(DATA_WIDTH-1 downto 0);

    ---------------------------------------------------------------------------
    -- Register bank access
    ---------------------------------------------------------------------------
    reg_addr  : out integer range 0 to 3;
    reg_we    : out std_logic;
    reg_wstrb : out std_logic_vector(DATA_WIDTH/8-1 downto 0);
    reg_wdata : out std_logic_vector(DATA_WIDTH-1 downto 0);
    reg_rdata : in  std_logic_vector(DATA_WIDTH-1 downto 0)
  );
end entity axi_peripheral_bus;

architecture rtl of axi_peripheral_bus is

  -- Internal memory bus records connecting axi_slave to axi_mem_arbiter
  signal mem_wr_cmd  : t_mem_wr_cmd;
  signal mem_wr_resp : t_mem_wr_resp;
  signal mem_rd_cmd  : t_mem_rd_cmd;
  signal mem_rd_resp : t_mem_rd_resp;

begin

  ---------------------------------------------------------------------------
  -- Global internal signals
  ---------------------------------------------------------------------------
  -- Write data/strobe passed through to register bank.
  -- axi_mem_arbiter uses these for port-A steering but does not expose them
  -- as separate outputs; pass them directly from the memory bus record.
  reg_wstrb <= mem_wr_cmd.strb;
  reg_wdata <= mem_wr_cmd.data;

  ---------------------------------------------------------------------------
  -- AXI4 protocol engine
  ---------------------------------------------------------------------------
  slave_inst : entity work.axi_slave
    generic map (
      ADDR_WIDTH => ADDR_WIDTH,
      DATA_WIDTH => DATA_WIDTH,
      ID_WIDTH   => ID_WIDTH
    )
    port map (
      axi_clk        => axi_clk,        aresetn     => aresetn,
      s_axi_in    => s_axi_in,    s_axi_out   => s_axi_out,
      mem_wr_cmd  => mem_wr_cmd,  mem_wr_resp => mem_wr_resp,
      mem_rd_cmd  => mem_rd_cmd,  mem_rd_resp => mem_rd_resp
    );

  ---------------------------------------------------------------------------
  -- Address decoder and port-A arbiter
  ---------------------------------------------------------------------------
  arbiter_inst : entity work.axi_mem_arbiter
    generic map (
      ADDR_WIDTH => ADDR_WIDTH,
      DATA_WIDTH => DATA_WIDTH
    )
    port map (
      mem_waddr     => mem_wr_cmd.addr,    mem_raddr     => mem_rd_cmd.addr,
      mem_wdata     => mem_wr_cmd.data,    mem_wstrb     => mem_wr_cmd.strb,
      mem_we        => mem_wr_cmd.we,      mem_rmw_re    => mem_wr_cmd.rmw_re,
      mem_rmw_we    => mem_wr_cmd.rmw_we,  mem_re        => mem_rd_cmd.re,
      bram_dout_a   => bram_dout_a,        pal_dout_a    => pal_dout_a,
      reg_rdata     => reg_rdata,
      mem_rd_nowait => mem_rd_resp.nowait,  mem_wr_nowait => mem_wr_resp.nowait,
      bram_en_a     => bram_en_a,          bram_we_a     => bram_we_a,
      bram_addr_a   => bram_addr_a,        bram_din_a    => bram_din_a,
      pal_en_a      => pal_en_a,           pal_we_a      => pal_we_a,
      pal_addr_a    => pal_addr_a,         pal_din_a     => pal_din_a,
      reg_addr      => reg_addr,           reg_we        => reg_we,
      mem_rdata     => mem_rd_resp.data,   mem_rmw_rdata => mem_wr_resp.rmw_rdata
    );

end architecture rtl;
