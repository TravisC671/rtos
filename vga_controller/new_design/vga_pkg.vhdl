--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- VGA Design Package                                                   vga_pkg
--
-- Consolidates all shared types and constants used across the VGA controller
-- hierarchy into a single package.  
--
-- Contents
-- --------
--   1. VGA timing constants    (640x400 @ 70 Hz pixel clock)
--   2. Display memory constants (framebuffer geometry and palette sizing)
--   3. AXI4 record types       (t_axi4_s2m, t_axi4_m2s)
--   4. Memory-bus record types  (t_mem_wr_cmd, t_mem_rd_cmd,
--                                t_mem_wr_resp, t_mem_rd_resp)
--   5. Register-array type      (t_reg_array)
--
-- AXI4 record convention
-- ----------------------
--   t_axi4_m2s  signals driven by the AXI master  (inputs  to the slave)
--   t_axi4_s2m  signals driven by the AXI slave   (outputs from the slave)
--   Ports declared as:
--     s_axi_in  : in  t_axi4_m2s   -- master drives
--     s_axi_out : out t_axi4_s2m   -- slave  drives
--   awready / wready / arready are in t_axi4_s2m because the slave drives
--   them (even though they are sometimes declared inout on entity boundaries
--   that need to read them back).
--
-- Memory-bus record convention
-- ----------------------------
--   t_mem_wr_cmd   write command from axi_slave to arbiter
--   t_mem_rd_cmd   read  command from axi_slave to arbiter
--   t_mem_wr_resp  write response from arbiter to axi_slave (rmw_rdata,
--                  wr_nowait)
--   t_mem_rd_resp  read  response from arbiter to axi_slave (rdata,
--                  rd_nowait)
--
-- Register-array type
-- -------------------
--   t_reg_array is an unconstrained array of 32-bit std_logic_vectors.
--   Use as:  signal regs : t_reg_array(0 to NUM_REGS-1);
--   Access:  regs(i)         -- full word
--            regs(i)(0)      -- bit 0
--
-- Compile order
-- -------------
--   This package must be compiled before every other file in the design.
--------------------------------------------------------------------------------

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

package vga_pkg is

  ---------------------------------------------------------------------------
  -- 1. VGA timing constants  (640x400 @ 70 Hz, 25.175 MHz pixel clock)
  ---------------------------------------------------------------------------
  constant H_DISPLAY     : integer := 640;
  constant H_FRONT_PORCH : integer := 16;
  constant H_SYNC_PULSE  : integer := 96;
  constant H_BACK_PORCH  : integer := 48;
  constant H_TOTAL       : integer := 800;

  constant V_DISPLAY     : integer := 400;
  constant V_FRONT_PORCH : integer := 12;
  constant V_SYNC_PULSE  : integer := 2;
  constant V_BACK_PORCH  : integer := 35;
  constant V_TOTAL       : integer := 449;

  -- Sync polarity (0 = active low, 1 = active high)
  constant H_SYNC_POL    : std_logic := '0';  -- negative
  constant V_SYNC_POL    : std_logic := '1';  -- positive (640x400 standard)

  ---------------------------------------------------------------------------
  -- 2. Display memory constants
  --    These describe the framebuffer and palette geometry and must not be
  --    changed independently of the timing constants above; a resolution
  --    change requires updating both sections together.
  ---------------------------------------------------------------------------

  -- Framebuffer resolution modes
  --   Mode 0 (mode_320x200='1'): 320x200 pixels, doubled 2x2 to fill 640x400
  --   Mode 1 (mode_320x200='0'): 640x400 pixels, native 1:1 mapping
  constant FB_WIDTH_LOW    : integer := 320;
  constant FB_HEIGHT_LOW   : integer := 200;
  constant FB_WIDTH_HIGH   : integer := 640;
  constant FB_HEIGHT_HIGH  : integer := 400;

  -- Framebuffer address bus width.  Sized for the larger 640x400 mode:
  --   640 x 400 = 256,000 bytes  =>  18-bit byte address (2^18 = 262,144)
  constant FB_ADDR_WIDTH   : integer := 18;

  -- Double-buffering layout for 320x200 mode (two packed buffers in BRAM)
  constant FB_320x200_SIZE  : integer := 64000;
  constant FB_BUFFER_0_BASE : integer := 0;
  constant FB_BUFFER_1_BASE : integer := 64000;

  -- BRAM depth in 32-bit words.  The binding constraint is 640x400 =
  -- 256,000 bytes = 64,000 words.  See vga_timing_pkg note for why Vivado
  -- still infers 64 RAMB36 despite the non-power-of-two depth.
  constant FB_BRAM_DEPTH   : integer := FB_WIDTH_HIGH * FB_HEIGHT_HIGH / 4;
  -- = 64,000 words = 256,000 bytes

  -- Colour indexing and palette
  constant PIXEL_BITS      : integer := 8;   -- 8-bit indexed colour
  constant PALETTE_BITS    : integer := 12;  -- 12-bit RGB (4:4:4)
  constant PALETTE_SIZE    : integer := 256; -- 256 palette entries

  ---------------------------------------------------------------------------
  -- 3. AXI4 record types
  --
  --    Subtypes below use unconstrained std_logic_vector so a single record
  --    definition covers any ADDR_WIDTH / DATA_WIDTH / ID_WIDTH.  Callers
  --    constrain at the signal declaration site, e.g.:
  --
  --    Widths are fixed at the design maximums (32-bit addr, 32-bit data,
  --    4-bit ID).  Narrower instantiations simply use the lower bits.
  --    This avoids unconstrained record elements which Vivado does not
  --    support even in VHDL-2008 mode.
  ---------------------------------------------------------------------------

  -- Signals driven by the AXI master (inputs to the slave entity)
  type t_axi4_m2s is record
    -- Write address channel
    awid    : std_logic_vector(3  downto 0);
    awaddr  : std_logic_vector(31 downto 0);
    awlen   : std_logic_vector(7  downto 0);
    awsize  : std_logic_vector(2  downto 0);
    awburst : std_logic_vector(1  downto 0);
    awlock  : std_logic;
    awcache : std_logic_vector(3  downto 0);
    awprot  : std_logic_vector(2  downto 0);
    awqos   : std_logic_vector(3  downto 0);
    awvalid : std_logic;
    -- Write data channel
    wdata   : std_logic_vector(31 downto 0);
    wstrb   : std_logic_vector(3  downto 0);
    wlast   : std_logic;
    wvalid  : std_logic;
    -- Write response channel
    bready  : std_logic;
    -- Read address channel
    arid    : std_logic_vector(3  downto 0);
    araddr  : std_logic_vector(31 downto 0);
    arlen   : std_logic_vector(7  downto 0);
    arsize  : std_logic_vector(2  downto 0);
    arburst : std_logic_vector(1  downto 0);
    arlock  : std_logic;
    arcache : std_logic_vector(3  downto 0);
    arprot  : std_logic_vector(2  downto 0);
    arqos   : std_logic_vector(3  downto 0);
    arvalid : std_logic;
    -- Read data channel
    rready  : std_logic;
  end record t_axi4_m2s;

  -- Signals driven by the AXI slave (outputs from the slave entity)
  type t_axi4_s2m is record
    -- Write address channel
    awready : std_logic;
    -- Write data channel
    wready  : std_logic;
    -- Write response channel
    bid     : std_logic_vector(3  downto 0);
    bresp   : std_logic_vector(1  downto 0);
    bvalid  : std_logic;
    -- Read address channel
    arready : std_logic;
    -- Read data channel
    rid     : std_logic_vector(3  downto 0);
    rdata   : std_logic_vector(31 downto 0);
    rresp   : std_logic_vector(1  downto 0);
    rlast   : std_logic;
    rvalid  : std_logic;
  end record t_axi4_s2m;

  ---------------------------------------------------------------------------
  -- 4. Internal memory-bus record types
  --
  --    These carry the signals between axi_slave and axi_mem_arbiter.
  --    Splitting write and read sides makes it easy to pass only the
  --    relevant half to entities that only handle one direction.
  ---------------------------------------------------------------------------

  -- Write command (axi_slave -> arbiter)
  type t_mem_wr_cmd is record
    addr   : std_logic_vector(31 downto 0);  -- byte address
    data   : std_logic_vector(31 downto 0);  -- write data
    strb   : std_logic_vector(3  downto 0);  -- byte enables
    we     : std_logic;                      -- full-word write strobe
    rmw_re : std_logic;                      -- RMW read  strobe
    rmw_we : std_logic;                      -- RMW write strobe
  end record t_mem_wr_cmd;

  -- Write response (arbiter -> axi_slave)
  type t_mem_wr_resp is record
    rmw_rdata : std_logic_vector(31 downto 0);  -- merged word for RMW write-back
    nowait    : std_logic;                      -- '1' = register bank (zero latency)
  end record t_mem_wr_resp;

  -- Read command (axi_slave -> arbiter)
  type t_mem_rd_cmd is record
    addr : std_logic_vector(31 downto 0);  -- byte address
    re   : std_logic;                      -- read strobe
  end record t_mem_rd_cmd;

  -- Read response (arbiter -> axi_slave)
  type t_mem_rd_resp is record
    data   : std_logic_vector(31 downto 0);  -- read data
    nowait : std_logic;                      -- '1' = register bank (zero latency)
  end record t_mem_rd_resp;

  ---------------------------------------------------------------------------
  -- 5. Register-array type
  --
  --    Used by generic_reg_bank and register_bank instead of flat packed
  --    std_logic_vector buses.  Each element is a full 32-bit word.
  --    Example:
  --      signal regs : t_reg_array(0 to 3);   -- four registers
  --      regs(2)(0) <= '1';                    -- set bit 0 of register 2
  ---------------------------------------------------------------------------
  type t_reg_array is array(natural range <>) of std_logic_vector(31 downto 0);

  ---------------------------------------------------------------------------
  -- Utility types and functions
  ---------------------------------------------------------------------------
  -- Selector subtypes and packing functions used by the AXI FSMs to build
  -- with/select expressions without ambiguous "&" overloads.
  subtype t_sel3 is std_logic_vector(2 downto 0);

  -- Pack two std_logic bits into a std_logic_vector(1 downto 0).
  function sel2(a, b : std_logic) return std_logic_vector;

  -- Pack three std_logic bits into a t_sel3.
  function sel3(a, b, c : std_logic) return t_sel3;

end package vga_pkg;

package body vga_pkg is

  function sel2(a, b : std_logic) return std_logic_vector is
    variable r : std_logic_vector(1 downto 0);
  begin
    r(1) := a;
    r(0) := b;
    return r;
  end function sel2;

  function sel3(a, b, c : std_logic) return t_sel3 is
    variable r : t_sel3;
  begin
    r(2) := a;
    r(1) := b;
    r(0) := c;
    return r;
  end function sel3;

end package body vga_pkg;
