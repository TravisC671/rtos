--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------


--------------------------------------------------------------------------------
-- AXI Memory Bus Arbiter / Address Decoder                     axi_mem_arbiter
--
-- Shared by vga_controller and axi_framebuffer.  Performs:
--
--   1. Three-way address decode from latched AXI addresses:
--        addr(18)='0'                  -> framebuffer BRAM
--        addr(18)='1', addr(9)='0'    -> palette RAM   (0x40000-0x401FF)
--        addr(18)='1', addr(9)='1'    -> register bank (0x40200-0x403FF)
--
--   2. Port-A enable/write/address/data steering for BRAM and palette RAM,
--      honouring write-FSM priority over read-FSM.
--
--   3. Read-data mux: selects mem_rdata (normal read) and mem_rmw_rdata
--      (RMW read-back) from the correct sub-module.
--
--   4. Zero-latency signalling: mem_rd_nowait and mem_wr_nowait are asserted
--      when the register bank is the target (combinational read/write).
--
--   5. Register-bank address and write-enable generation, with the correct
--      guard on reg_we (see note below).
--
-- NOTE on reg_addr mux guard
-- --------------------------
-- sel_reg_w persists from the last write transaction because it is decoded
-- from mem_waddr which is held after the transaction ends.  Using sel_reg_w
-- alone as the mux selector causes reg_addr to track mem_waddr during
-- subsequent reads, producing wrong read data.  Gating with mem_we ensures
-- the write address is used only while a write beat is actually executing.
--
-- Ports
-- -----
-- Control inputs (from axi_slave memory bus)
--   mem_waddr      latched write byte address  (ADDR_WIDTH bits)
--   mem_raddr      latched read  byte address  (ADDR_WIDTH bits)
--   mem_wdata      write data bus              (DATA_WIDTH bits)
--   mem_wstrb      byte enables                (DATA_WIDTH/8 bits)
--   mem_we         full-word write strobe
--   mem_rmw_re     RMW read  strobe
--   mem_rmw_we     RMW write strobe
--   mem_re         read strobe
--
-- BRAM port-A data (from framebuffer_bram)
--   bram_dout_a    registered word output from port A
--
-- Palette RAM port-A data (from palette_ram)
--   pal_dout_a     registered word output from port A
--
-- Register bank read data (from register_bank, combinational)
--   reg_rdata      32-bit combinational read data
--
-- Decode outputs (to FSMs, datapaths, and sub-modules)
--   sel_fb_w/r     framebuffer selected for write/read
--   sel_pal_w/r    palette     selected for write/read
--   sel_reg_w/r    reg bank    selected for write/read
--   mem_rd_nowait  '1' when reg bank is the read target
--   mem_wr_nowait  '1' when reg bank is the write target
--
-- BRAM port-A control
--   bram_en_a, bram_we_a
--   bram_addr_a    (FB_ADDR_WIDTH bits)
--   bram_din_a     (DATA_WIDTH bits)
--
-- Palette port-A control
--   pal_en_a, pal_we_a
--   pal_addr_a     (7 bits: byte addr[8:2])
--   pal_din_a      (DATA_WIDTH bits)
--
-- Register bank access
--   reg_addr       word index 0-3
--   reg_we         write enable
--
-- Memory bus responses
--   mem_rdata      read data mux to axi_slave
--   mem_rmw_rdata  RMW read-back data to axi_slave
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.vga_pkg.all;

entity axi_mem_arbiter is
  generic (
    ADDR_WIDTH : integer := 32;
    DATA_WIDTH : integer := 32
  );
  port (
    ---------------------------------------------------------------------------
    -- Memory bus inputs (from axi_slave)
    ---------------------------------------------------------------------------
    mem_waddr  : in std_logic_vector(ADDR_WIDTH-1   downto 0);
    mem_raddr  : in std_logic_vector(ADDR_WIDTH-1   downto 0);
    mem_wdata  : in std_logic_vector(DATA_WIDTH-1   downto 0);
    mem_wstrb  : in std_logic_vector(DATA_WIDTH/8-1 downto 0);
    mem_we     : in std_logic;
    mem_rmw_re : in std_logic;
    mem_rmw_we : in std_logic;
    mem_re     : in std_logic;

    ---------------------------------------------------------------------------
    -- Sub-module read-data inputs
    ---------------------------------------------------------------------------
    bram_dout_a : in std_logic_vector(DATA_WIDTH-1 downto 0);
    pal_dout_a  : in std_logic_vector(DATA_WIDTH-1 downto 0);
    reg_rdata   : in std_logic_vector(DATA_WIDTH-1 downto 0);

    ---------------------------------------------------------------------------
    -- Zero-latency signalling
    ---------------------------------------------------------------------------
    mem_rd_nowait : out std_logic;
    mem_wr_nowait : out std_logic;

    ---------------------------------------------------------------------------
    -- BRAM port-A control
    ---------------------------------------------------------------------------
    bram_en_a   : out std_logic;
    bram_we_a   : out std_logic;
    bram_addr_a : out std_logic_vector(FB_ADDR_WIDTH-1 downto 0);
    bram_din_a  : out std_logic_vector(DATA_WIDTH-1 downto 0);

    ---------------------------------------------------------------------------
    -- Palette port-A control
    ---------------------------------------------------------------------------
    pal_en_a    : out std_logic;
    pal_we_a    : out std_logic;
    pal_addr_a  : out std_logic_vector(6 downto 0);
    pal_din_a   : out std_logic_vector(DATA_WIDTH-1 downto 0);

    ---------------------------------------------------------------------------
    -- Register bank access
    ---------------------------------------------------------------------------
    reg_addr : out integer range 0 to 3;
    reg_we   : out std_logic;

    ---------------------------------------------------------------------------
    -- Memory bus responses to axi_slave
    ---------------------------------------------------------------------------
    mem_rdata     : out std_logic_vector(DATA_WIDTH-1 downto 0);
    mem_rmw_rdata : out std_logic_vector(DATA_WIDTH-1 downto 0)
  );
end entity axi_mem_arbiter;

architecture rtl of axi_mem_arbiter is

  -- Internal copies of decode outputs (needed to reference in multiple
  -- downstream expressions without re-evaluating the comparisons)
  signal sel_fb_w_i  : std_logic;
  signal sel_pal_w_i : std_logic;
  signal sel_reg_w_i : std_logic;
  signal sel_fb_r_i  : std_logic;
  signal sel_pal_r_i : std_logic;
  signal sel_reg_r_i : std_logic;

  -- Address-steering helpers
  signal w_addr_active  : std_logic; -- any write-side memory cycle
  signal fb_write_beat  : std_logic; -- full-word write to framebuffer
  signal fb_rmw_write   : std_logic; -- RMW write phase to framebuffer
  signal pal_write_beat : std_logic; -- full-word write to palette
  signal pal_rmw_write  : std_logic; -- RMW write phase to palette
  signal reg_we_i       : std_logic;

  ---------------------------------------------------------------------------
  -- Encoded selectors for with/select muxes
  --
  -- rsel: 2-bit read-target selector derived from one-hot sel_*_r_i.
  --   "00" -> framebuffer   "01" -> palette   "10" -> register bank
  --
  -- wsel: 2-bit write-target selector derived from one-hot sel_*_w_i.
  --   "00" -> framebuffer   "01" -> palette   (reg bank never goes to RMW)
  --
  ---------------------------------------------------------------------------
  signal rsel   : std_logic_vector(1 downto 0);
  signal wsel   : std_logic_vector(1 downto 0);

begin

  ---------------------------------------------------------------------------
  -- Address decode
  -- Boundary bits:
  --   bit 18 discriminates framebuffer (0) from palette/reg (1)
  --   bit 9  discriminates palette (0) from register bank (1) within the
  --          upper half (0x40000-0x403FF)
  --          0x40000-0x401FF -> palette  (bit 9 = 0)
  --          0x40200-0x403FF -> reg bank (bit 9 = 1)
  ---------------------------------------------------------------------------
  sel_fb_w_i  <= not mem_waddr(18);
  sel_pal_w_i <=     mem_waddr(18) and not mem_waddr(9);
  sel_reg_w_i <=     mem_waddr(18) and     mem_waddr(9);

  sel_fb_r_i  <= not mem_raddr(18);
  sel_pal_r_i <=     mem_raddr(18) and not mem_raddr(9);
  sel_reg_r_i <=     mem_raddr(18) and     mem_raddr(9);

  mem_rd_nowait <= sel_reg_r_i;
  mem_wr_nowait <= sel_reg_w_i;

  ---------------------------------------------------------------------------
  -- Encoded selectors for with/select data muxes
  ---------------------------------------------------------------------------
  -- rsel: 2-bit read-target selector derived from one-hot sel_*_r_i.
  --   "00" -> framebuffer   "01" -> palette   "10" -> register bank
  rsel <= "10" when sel_reg_r_i = '1' else
          "01" when sel_pal_r_i = '1' else
          "00";  -- framebuffer (default / sel_fb_r_i)

  -- wsel: 2-bit write-target selector derived from one-hot sel_*_w_i.
  --   "00" -> framebuffer   "01" -> palette   "10" -> register bank
  wsel <= "10" when sel_reg_w_i = '1' else
          "01" when sel_pal_w_i = '1' else
          "00";  -- framebuffer (default / sel_fb_w_i)

  ---------------------------------------------------------------------------
  -- Address-steering helpers
  ---------------------------------------------------------------------------
  w_addr_active  <= mem_we or mem_rmw_re or mem_rmw_we;
  fb_write_beat  <= mem_we     and sel_fb_w_i;
  fb_rmw_write   <= mem_rmw_we and sel_fb_w_i;
  pal_write_beat <= mem_we     and sel_pal_w_i;
  pal_rmw_write  <= mem_rmw_we and sel_pal_w_i;

  ---------------------------------------------------------------------------
  -- Framebuffer BRAM port-A control
  ---------------------------------------------------------------------------
  bram_en_a <=
    '1' when fb_write_beat = '1'                       else
    '1' when mem_rmw_re    = '1' and sel_fb_w_i = '1' else
    '1' when fb_rmw_write  = '1'                       else
    '1' when mem_re        = '1' and sel_fb_r_i = '1' else
    '0';

  bram_we_a <=
    '1' when fb_write_beat = '1' else
    '1' when fb_rmw_write  = '1' else
    '0';

  bram_addr_a <=
    mem_waddr(FB_ADDR_WIDTH-1 downto 0) when w_addr_active = '1' else
    mem_raddr(FB_ADDR_WIDTH-1 downto 0);

  bram_din_a <= mem_wdata;

  ---------------------------------------------------------------------------
  -- Palette RAM port-A control
  -- Word address: 32-bit words so word_addr = byte_addr[8:2]
  ---------------------------------------------------------------------------
  pal_en_a <=
    '1' when pal_write_beat = '1'                        else
    '1' when mem_rmw_re     = '1' and sel_pal_w_i = '1' else
    '1' when pal_rmw_write  = '1'                        else
    '1' when mem_re         = '1' and sel_pal_r_i = '1' else
    '0';

  pal_we_a <=
    '1' when pal_write_beat = '1' else
    '1' when pal_rmw_write  = '1' else
    '0';

  pal_addr_a <=
    mem_waddr(8 downto 2) when w_addr_active = '1' else
    mem_raddr(8 downto 2);

  pal_din_a <= mem_wdata;

  ---------------------------------------------------------------------------
  -- Register bank access
  -- reg_addr mux is gated on mem_we (not sel_reg_w) to prevent stale
  -- waddr from overriding raddr between transactions.
  ---------------------------------------------------------------------------
  reg_we_i <= mem_we and sel_reg_w_i;
  reg_we   <= reg_we_i;
  reg_addr <=
    to_integer(unsigned(mem_waddr(3 downto 2))) when reg_we_i = '1' else
    to_integer(unsigned(mem_raddr(3 downto 2)));

  ---------------------------------------------------------------------------
  -- Read-data muxes back to axi_slave
  ---------------------------------------------------------------------------
  -- mem_rdata: three-input mux on rsel.
  with rsel select mem_rdata <=
    bram_dout_a when "00",   -- framebuffer
    pal_dout_a  when "01",   -- palette
    reg_rdata   when others; -- "10" register bank

  -- mem_rmw_rdata: three-input mux on wsel (reg bank never needs RMW).
  with wsel select mem_rmw_rdata <=
    bram_dout_a  when "00",   -- framebuffer
    pal_dout_a   when "01",   -- palette
    (others=>'0') when others; -- "10" register bank (RAZ; RMW never issued)

end architecture rtl;
