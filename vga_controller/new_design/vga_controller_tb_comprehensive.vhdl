--------------------------------------------------------------------------------
-- Copyright (c) 2026 Larry D. Pyeatt
-- All rights reserved.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- VGA Controller Comprehensive Testbench
-- Tests all major functionality of the refactored vga_controller, including:
--   - Control register writes (MODE bit, RAZ/WI for unused bits)
--   - Framebuffer byte/halfword/word writes using RMW and full-word paths
--   - Framebuffer multi-byte reads
--   - Palette writes: 256 entries stored as 128 x 32-bit words
--     (two 12-bit entries per word, upper 4 bits of each halfword unused)
--   - Mode switching (320x200 / 640x400)
--   - Double buffering (Display Buffer Register)
--   - Interrupt enable, assertion on VSYNC, and W1C clear
--   - VGA timing: VSYNC and HSYNC pulse counting
--   - Address decoding (framebuffer / palette / register bank)
--
-- Address map under test:
--   0x00000..0x3E7FF  framebuffer_bram  (250 KB, 640x400 bytes)
--   0x40000..0x401FF  palette_ram       (512 B, 128 x 32-bit words)
--   0x40200           Control Register
--   0x40204           Display Buffer Register
--   0x40208           IRQ Enable Register
--   0x4020C           IRQ Clear/Status Register
--
-- NOTE: axi_write computes wstrb from non-zero bytes of wdata.
-- Data must therefore be placed in the correct byte lane for the target
-- byte address so that the strobe covers the right bytes.
-- For sub-word writes the unused lanes must be zero.
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use STD.TEXTIO.ALL;

use work.AXI_master_simulation.all;
use work.vga_pkg.all;

entity vga_controller_tb_comprehensive is
end vga_controller_tb_comprehensive;

architecture Behavioral of vga_controller_tb_comprehensive is
    -- Constants
    constant CLK_PERIOD_PIXEL : time := 39.7 ns;  -- 25.175 MHz
    constant CLK_PERIOD_AXI   : time := 10 ns;    -- 100 MHz
    
    -- 32-bit addresses to match AXI_master_simulation; lower 20 bits connect to DUT
    constant CTRL_REG_ADDR    : std_logic_vector(31 downto 0) := x"00040200";
    constant DISP_BUF_ADDR    : std_logic_vector(31 downto 0) := x"00040204";
    constant IRQ_EN_ADDR      : std_logic_vector(31 downto 0) := x"00040208";
    constant IRQ_CLR_ADDR     : std_logic_vector(31 downto 0) := x"0004020C";
    constant PALETTE_BASE     : std_logic_vector(31 downto 0) := x"00040000";
    constant FB_BASE          : std_logic_vector(31 downto 0) := x"00000000";
    constant FB_BUFFER_1_BASE : std_logic_vector(31 downto 0) := x"0000FA00";
    
    -- Signals
    signal pixel_clk    : std_logic := '0';
    signal axi_clk      : std_logic := '0';
    signal resetn       : std_logic := '0';
    
    signal vga_hsync    : std_logic;
    signal vga_vsync    : std_logic;
    signal vga_video_on : std_logic;
    signal red,green,blue : std_logic_vector(3 downto 0);
    signal irq          : std_logic;

    -- AXI signals (32-bit addr to match package; lower 20 bits wired to DUT)
    signal s_axi_awid    : std_logic_vector(3 downto 0)  := (others => '0');
    signal s_axi_awaddr  : std_logic_vector(31 downto 0) := (others => '0');
    signal s_axi_awlen   : std_logic_vector(7 downto 0)  := (others => '0');
    signal s_axi_awsize  : std_logic_vector(2 downto 0)  := "010";
    signal s_axi_awburst : std_logic_vector(1 downto 0)  := "01";
    signal s_axi_awlock  : std_logic := '0';
    signal s_axi_awcache : std_logic_vector(3 downto 0)  := (others => '0');
    signal s_axi_awprot  : std_logic_vector(2 downto 0)  := (others => '0');
    signal s_axi_awqos   : std_logic_vector(3 downto 0)  := (others => '0');
    signal s_axi_awvalid : std_logic := '0';
    signal s_axi_awready : std_logic;
    signal s_axi_wdata   : std_logic_vector(31 downto 0) := (others => '0');
    signal s_axi_wstrb   : std_logic_vector(3 downto 0)  := (others => '0');
    signal s_axi_wlast   : std_logic := '1';
    signal s_axi_wvalid  : std_logic := '0';
    signal s_axi_wready  : std_logic;
    signal s_axi_bid     : std_logic_vector(3 downto 0);
    signal s_axi_bresp   : std_logic_vector(1 downto 0);
    signal s_axi_bvalid  : std_logic;
    signal s_axi_bready  : std_logic := '0';
    signal s_axi_arid    : std_logic_vector(3 downto 0)  := (others => '0');
    signal s_axi_araddr  : std_logic_vector(31 downto 0) := (others => '0');
    signal s_axi_arlen   : std_logic_vector(7 downto 0)  := (others => '0');
    signal s_axi_arsize  : std_logic_vector(2 downto 0)  := "010";
    signal s_axi_arburst : std_logic_vector(1 downto 0)  := "01";
    signal s_axi_arlock  : std_logic := '0';
    signal s_axi_arcache : std_logic_vector(3 downto 0)  := (others => '0');
    signal s_axi_arprot  : std_logic_vector(2 downto 0)  := (others => '0');
    signal s_axi_arqos   : std_logic_vector(3 downto 0)  := (others => '0');
    signal s_axi_arvalid : std_logic := '0';
    signal s_axi_arready : std_logic;
    signal s_axi_rid     : std_logic_vector(3 downto 0);
    signal s_axi_rdata   : std_logic_vector(31 downto 0);
    signal s_axi_rresp   : std_logic_vector(1 downto 0);
    signal s_axi_rlast   : std_logic;
    signal s_axi_rvalid  : std_logic;
    signal s_axi_rready  : std_logic := '0';

    -- AXI record signals assembled from flat signals above
    signal dut_axi_in  : t_axi4_m2s;
    signal dut_axi_out : t_axi4_s2m;
    
    -- Test tracking
    signal test_count    : integer := 0;
    signal pass_count    : integer := 0;
    signal fail_count    : integer := 0;
    signal vsync_count   : integer := 0;
    signal hsync_count   : integer := 0;
    signal vsync_prev    : std_logic := '0';
    
    -- Read data signal for AXI read operations
    signal rdata         : std_logic_vector(31 downto 0);

    -- Timing measurement signals for Suite 11
    signal vsync_time_1  : time := 0 ns;
    signal vsync_time_2  : time := 0 ns;
    signal hsync_time_1  : time := 0 ns;
    signal hsync_time_2  : time := 0 ns;

    -- IRQ period measurement for Suite 13
    signal irq_time_1    : time := 0 ns;
    signal irq_time_2    : time := 0 ns;
    signal irq_time_3    : time := 0 ns;
    signal irq_period_1  : time := 0 ns;
    signal irq_period_2  : time := 0 ns;

    -- Burst data arrays for Suite 12
    signal burst_wr_data : slv32_array(0 to 7);
    signal burst_rd_data : slv32_array(0 to 7);
    
    procedure check(
        condition : in boolean;
        test_name : in string;
        signal tc : inout integer;
        signal pc : inout integer;
        signal fc : inout integer
    ) is
        variable l : line;
    begin
        tc <= tc + 1;
        if condition then
            pc <= pc + 1;
            write(l, string'("PASS: ") & test_name);
        else
            fc <= fc + 1;
            write(l, string'("FAIL: ") & test_name);
        end if;
        writeline(output, l);
    end procedure;

begin
    -- DUT instantiation
    -- Assemble AXI record from flat testbench signals
    dut_axi_in.awid    <= s_axi_awid;
    dut_axi_in.awaddr  <= s_axi_awaddr;
    dut_axi_in.awlen   <= s_axi_awlen;
    dut_axi_in.awsize  <= s_axi_awsize;
    dut_axi_in.awburst <= s_axi_awburst;
    dut_axi_in.awlock  <= s_axi_awlock;
    dut_axi_in.awcache <= s_axi_awcache;
    dut_axi_in.awprot  <= s_axi_awprot;
    dut_axi_in.awqos   <= s_axi_awqos;
    dut_axi_in.awvalid <= s_axi_awvalid;
    dut_axi_in.wdata   <= s_axi_wdata;
    dut_axi_in.wstrb   <= s_axi_wstrb;
    dut_axi_in.wlast   <= s_axi_wlast;
    dut_axi_in.wvalid  <= s_axi_wvalid;
    dut_axi_in.bready  <= s_axi_bready;
    dut_axi_in.arid    <= s_axi_arid;
    dut_axi_in.araddr  <= s_axi_araddr;
    dut_axi_in.arlen   <= s_axi_arlen;
    dut_axi_in.arsize  <= s_axi_arsize;
    dut_axi_in.arburst <= s_axi_arburst;
    dut_axi_in.arlock  <= s_axi_arlock;
    dut_axi_in.arcache <= s_axi_arcache;
    dut_axi_in.arprot  <= s_axi_arprot;
    dut_axi_in.arqos   <= s_axi_arqos;
    dut_axi_in.arvalid <= s_axi_arvalid;
    dut_axi_in.rready  <= s_axi_rready;

    -- Decompose AXI record back to flat testbench signals
    s_axi_awready <= dut_axi_out.awready;
    s_axi_wready  <= dut_axi_out.wready;
    s_axi_bid     <= dut_axi_out.bid;
    s_axi_bresp   <= dut_axi_out.bresp;
    s_axi_bvalid  <= dut_axi_out.bvalid;
    s_axi_arready <= dut_axi_out.arready;
    s_axi_rid     <= dut_axi_out.rid;
    s_axi_rdata   <= dut_axi_out.rdata;
    s_axi_rresp   <= dut_axi_out.rresp;
    s_axi_rlast   <= dut_axi_out.rlast;
    s_axi_rvalid  <= dut_axi_out.rvalid;

    dut : entity work.vga_controller
        generic map (
            AXI_ADDR_WIDTH => 32,
            AXI_DATA_WIDTH => 32,
            AXI_ID_WIDTH   => 4
        )
        port map (
            axi_clk    => axi_clk,
            aresetn    => resetn,
            pixel_clk  => pixel_clk,
            s_axi_in   => dut_axi_in,
            s_axi_out  => dut_axi_out,
            hsync      => vga_hsync,
            vsync      => vga_vsync,
            video_on   => vga_video_on,
            red        => red,
            green      => green,
            blue       => blue,
            irq        => irq
        );
    
    -- Clock generation
    pixel_clk <= not pixel_clk after CLK_PERIOD_PIXEL / 2;
    axi_clk   <= not axi_clk after CLK_PERIOD_AXI / 2;
    
    -- VSYNC counter
    process(pixel_clk)
    begin
        if rising_edge(pixel_clk) then
            vsync_prev <= vga_vsync;
            if vga_vsync = '1' and vsync_prev = '0' then
                vsync_count <= vsync_count + 1;
            end if;
            if vga_hsync = '0' then  -- H_SYNC_POL='0': active-low
                hsync_count <= hsync_count + 1;
            end if;
        end if;
    end process;
    
    -- Test stimulus
    process
        variable l : line;
    begin
        -- Reset
        resetn <= '0';
        wait for 100 ns;
        resetn <= '1';
        wait for 100 ns;
        
        write(l, string'("=== VGA Controller Comprehensive Test Suite ==="));
        writeline(output, l);
        
        ----------------------------------------------------------------------
        -- TEST SUITE 1: Register Read/Write with Different Byte Strobes
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 1: Control Register (RAZ/WI bits, MODE bit) ==="));
        writeline(output, l);
        
        -- Test 1.1: Set MODE=1.
        -- axi_write derives wstrb from non-zero bytes of wdata, so data must be
        -- placed in the correct byte lane.  CTRL_REG is word-aligned; byte 0
        -- contains MODE.  Write 0x00000001 so only byte 0 is non-zero -> strobe "0001".
        axi_write(CTRL_REG_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        axi_read(CTRL_REG_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(0) = '1', "Control reg: MODE bit set", test_count, pass_count, fail_count);
        check(rdata(31 downto 1) = std_logic_vector(to_unsigned(0,31)),
              "Control reg: reserved bits RAZ", test_count, pass_count, fail_count);

        -- Test 1.2: Verify MODE=1 persists across a second read.
        axi_read(CTRL_REG_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(0) = '1', "Control reg: MODE still set after re-read", test_count, pass_count, fail_count);
        
        ----------------------------------------------------------------------
        -- TEST SUITE 2: Framebuffer Multi-Byte Write Tests
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 2: Framebuffer Multi-Byte Writes ==="));
        writeline(output, l);
        
        -- Test 2.1: Single byte write (strb = 0x1)
        axi_write(FB_BASE, x"AA",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);


        wait for 50 ns;
        axi_read(FB_BASE, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
                
        check(rdata(7 downto 0) = x"AA", "FB single byte write", test_count, pass_count, fail_count);
        
        -- Test 2.2: Byte 1 only (strb = 0x2).
        -- Pass 8-bit data x"BB" at addr+1; procedure places it in wdata(15:8),
        -- strobe "0010", word-aligns awaddr to FB_BASE.
        -- axi_read masks addr to word boundary, returns full word: BB in rdata(15:8).
        axi_write(std_logic_vector(unsigned(FB_BASE) + 1), x"BB",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 50 ns;
        axi_read(std_logic_vector(unsigned(FB_BASE) + 1), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(15 downto 8) = x"BB", "FB byte 1 write", test_count, pass_count, fail_count);
        
        -- Test 2.3: Halfword write (strb = 0x3).
        -- Pass 16-bit data x"DDCC" at addr+100 (word-aligned, addr(1:0)="00");
        -- procedure places it in wdata(15:0), strobe "0011".
        -- axi_read returns full word: CC in rdata(7:0), DD in rdata(15:8).
        axi_write(std_logic_vector(unsigned(FB_BASE) + 100), x"DDCC",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 100 ns;
        axi_read(std_logic_vector(unsigned(FB_BASE) + 100), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(7 downto 0)  = x"CC", "FB halfword write - byte 0", test_count, pass_count, fail_count);
        check(rdata(15 downto 8) = x"DD", "FB halfword write - byte 1", test_count, pass_count, fail_count);
        
        -- Test 2.4: Word write (strb = 0xF) - should write 4 sequential bytes
        axi_write(std_logic_vector(unsigned(FB_BASE) + 200), x"44332211",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);


        wait for 200 ns;  -- Allow multi-byte transfer (4 bytes)
        -- axi_read always word-aligns; one read returns all four bytes.
        axi_read(std_logic_vector(unsigned(FB_BASE) + 200), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(7 downto 0)   = x"11", "FB word write - byte 0", test_count, pass_count, fail_count);
        check(rdata(15 downto 8)  = x"22", "FB word write - byte 1", test_count, pass_count, fail_count);
        check(rdata(23 downto 16) = x"33", "FB word write - byte 2", test_count, pass_count, fail_count);
        check(rdata(31 downto 24) = x"44", "FB word write - byte 3", test_count, pass_count, fail_count);
        
        -- Test 2.5: Non-contiguous byte writes (bytes 0 and 2 of word at offset 300).
        -- axi_write can't generate strobe "0101" directly; use two 8-bit writes.
        -- Write BB to byte 0 (addr+300), AA to byte 2 (addr+302).
        axi_write(std_logic_vector(unsigned(FB_BASE) + 300), x"BB",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        axi_write(std_logic_vector(unsigned(FB_BASE) + 302), x"AA",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 150 ns;
        axi_read(std_logic_vector(unsigned(FB_BASE) + 300), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(7 downto 0)   = x"BB", "FB non-contiguous write - byte 0", test_count, pass_count, fail_count);
        check(rdata(23 downto 16) = x"AA", "FB non-contiguous write - byte 2", test_count, pass_count, fail_count);
        
        -- Test 2.6: Upper halfword write (strb = 0xC, bytes 2 and 3).
        -- Pass 16-bit data x"FFEE" at addr+402 (addr(1:0)="10");
        -- procedure places it in wdata(31:16), strobe "1100".
        -- Read back the full word: EE in rdata(23:16), FF in rdata(31:24).
        axi_write(std_logic_vector(unsigned(FB_BASE) + 402), x"FFEE",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 150 ns;
        axi_read(std_logic_vector(unsigned(FB_BASE) + 402), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(23 downto 16) = x"EE", "FB upper halfword - byte 2", test_count, pass_count, fail_count);
        check(rdata(31 downto 24) = x"FF", "FB upper halfword - byte 3", test_count, pass_count, fail_count);
        
        ----------------------------------------------------------------------
        -- TEST SUITE 2.5: Framebuffer Multi-Byte Read Tests
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 2.5: Framebuffer Multi-Byte Reads ==="));
        writeline(output, l);
        
        -- Setup test data with known pattern
        axi_write(std_logic_vector(unsigned(FB_BASE) + 500), x"44332211",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);


        wait for 200 ns;
        
        -- Test 2.5.1: Byte read (should return byte 0)
        axi_read(std_logic_vector(unsigned(FB_BASE) + 500), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
                
        check(rdata(7 downto 0) = x"11", "FB byte read", test_count, pass_count, fail_count);
        
        -- Test 2.5.2: Halfword read.
        -- axi_read word-aligns the address; reading at +500 returns the full 32-bit
        -- word written above.  All byte lanes visible directly in rdata.
        axi_read(std_logic_vector(unsigned(FB_BASE) + 500), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(7 downto 0)  = x"11", "FB halfword read - byte 0", test_count, pass_count, fail_count);
        check(rdata(15 downto 8) = x"22", "FB halfword read - byte 1", test_count, pass_count, fail_count);
        
        -- Test 2.5.3: Word read (should return all 4 bytes in little-endian)
        axi_read(std_logic_vector(unsigned(FB_BASE) + 500), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
                
        wait for 200 ns;  -- Allow multi-byte read to complete
        check(rdata(7 downto 0) = x"11", "FB word read - byte 0", test_count, pass_count, fail_count);
        check(rdata(15 downto 8) = x"22", "FB word read - byte 1", test_count, pass_count, fail_count);
        check(rdata(23 downto 16) = x"33", "FB word read - byte 2", test_count, pass_count, fail_count);
        check(rdata(31 downto 24) = x"44", "FB word read - byte 3", test_count, pass_count, fail_count);
        
        -- Test 2.5.4: Verify different address also works
        axi_write(std_logic_vector(unsigned(FB_BASE) + 1000), x"AABBCCDD",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);


        wait for 200 ns;
        axi_read(std_logic_vector(unsigned(FB_BASE) + 1000), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
                
        wait for 200 ns;
        check(rdata(7 downto 0)   = x"DD", "FB word read - byte 0", test_count, pass_count, fail_count);
        check(rdata(15 downto 8)  = x"CC", "FB word read - byte 1", test_count, pass_count, fail_count);
        check(rdata(23 downto 16) = x"BB", "FB word read - byte 2", test_count, pass_count, fail_count);
        check(rdata(31 downto 24) = x"AA", "FB word read - byte 3", test_count, pass_count, fail_count);
        
        ----------------------------------------------------------------------
        -- TEST SUITE 3: Palette Tests
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 3: Palette Tests ==="));
        writeline(output, l);
        
        -- Palette: 128 x 32-bit words at 0x40000-0x401FF.
        -- Word N holds entry 2N in bits[15:0] and entry 2N+1 in bits[31:16].
        -- Upper 4 bits of each halfword are unused (RAZ/WI), lower 12 = RGB.

        -- Test 3.1: Write entries 0 (0xABC) and 1 (0xDEF) in one 32-bit word.
        -- All four bytes non-zero -> strobe "1111" -> full word written.
        axi_write(PALETTE_BASE, x"0DEF0ABC",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 50 ns;
        axi_read(PALETTE_BASE, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(11 downto 0)  = x"ABC", "Palette entry 0 (even, bits[11:0])",  test_count, pass_count, fail_count);
        check(rdata(27 downto 16) = x"DEF", "Palette entry 1 (odd, bits[27:16])",  test_count, pass_count, fail_count);

        -- Test 3.2: Write entries 2 (0x123) and 3 (0x456) at word offset +4.
        axi_write(std_logic_vector(unsigned(PALETTE_BASE) + 4), x"04560123",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 50 ns;
        axi_read(std_logic_vector(unsigned(PALETTE_BASE) + 4), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(11 downto 0)  = x"123", "Palette entry 2 (even, bits[11:0])",  test_count, pass_count, fail_count);
        check(rdata(27 downto 16) = x"456", "Palette entry 3 (odd, bits[27:16])",  test_count, pass_count, fail_count);
        
        ----------------------------------------------------------------------
        -- TEST SUITE 4: Mode Switching
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 4: Mode Switching ==="));
        writeline(output, l);
        
        -- Test 4.1: Set 320x200 mode
        axi_write(CTRL_REG_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);


        axi_read(CTRL_REG_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
                
        check(rdata(0) = '1', "320x200 mode set", test_count, pass_count, fail_count);
        
        -- Test 4.2: Set 640x400 mode
        axi_write(CTRL_REG_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);


        axi_read(CTRL_REG_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
                
        check(rdata(0) = '0', "640x400 mode set", test_count, pass_count, fail_count);
        
        ----------------------------------------------------------------------
        -- TEST SUITE 5: Double Buffering
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 5: Double Buffering ==="));
        writeline(output, l);
        
        -- Enable 320x200 mode first
        axi_write(CTRL_REG_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        
        -- Test 5.1: Select buffer 0
        axi_write(DISP_BUF_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);


        axi_read(DISP_BUF_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
                
        check(rdata(0) = '0', "Display buffer 0 selected", test_count, pass_count, fail_count);
        
        -- Test 5.2: Select buffer 1
        axi_write(DISP_BUF_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);


        axi_read(DISP_BUF_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
                
        check(rdata(0) = '1', "Display buffer 1 selected", test_count, pass_count, fail_count);
        
        -- Test 5.3: Write to both buffers and verify isolation
        axi_write(FB_BASE, x"00000055",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);


        axi_write(FB_BUFFER_1_BASE, x"000000AA",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);


        wait for 50 ns;
        axi_read(FB_BASE, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
                
        check(rdata(7 downto 0) = x"55", "Buffer 0 data", test_count, pass_count, fail_count);
        axi_read(FB_BUFFER_1_BASE, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
                
        check(rdata(7 downto 0) = x"AA", "Buffer 1 data", test_count, pass_count, fail_count);
        
        ----------------------------------------------------------------------
        -- TEST SUITE 6: Interrupt Tests
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 6: Interrupt Tests ==="));
        writeline(output, l);
        
        -- Test 6.1: Enable interrupt
        axi_write(IRQ_EN_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);


        axi_read(IRQ_EN_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
                
        check(rdata(0) = '1', "Interrupt enable set", test_count, pass_count, fail_count);
        
        -- Test 6.2: Wait for VSYNC and check interrupt
        wait until rising_edge(vga_vsync);
        wait for 1 us;  -- Allow interrupt to propagate
        check(irq = '1', "Interrupt asserted on VSYNC", test_count, pass_count, fail_count);
        
        -- Test 6.3: Clear interrupt
        axi_write(IRQ_CLR_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk); 

        wait for 100 ns;
        check(irq = '0', "Interrupt cleared", test_count, pass_count, fail_count);
        
        ----------------------------------------------------------------------
        -- TEST SUITE 7: VGA Timing Verification
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 7: VGA Timing ==="));
        writeline(output, l);
        
        -- Wait for a few frames
        wait until rising_edge(vga_vsync);
        wait until rising_edge(vga_vsync);
        wait until rising_edge(vga_vsync);
        
        check(vsync_count >= 3, "Multiple VSYNC pulses detected", test_count, pass_count, fail_count);
        check(hsync_count > 1000, "Multiple HSYNC pulses detected", test_count, pass_count, fail_count);
        
        ----------------------------------------------------------------------
        -- TEST SUITE 8: Register Bank - Remaining Registers RAZ/WI
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 8: Display Buffer and IRQ Ctrl RAZ/WI ==="));
        writeline(output, l);

        -- Test 8.1: Display Buffer register reserved bits RAZ/WI
        axi_write(DISP_BUF_ADDR, x"DEADBEEF",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        axi_read(DISP_BUF_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(0) = '1', "DispBuf: BUF bit set by 0xDEADBEEF", test_count, pass_count, fail_count);
        check(rdata(31 downto 1) = std_logic_vector(to_unsigned(0,31)),
              "DispBuf: reserved bits RAZ", test_count, pass_count, fail_count);

        -- Test 8.2: IRQ Enable register reserved bits RAZ/WI
        -- Only bit 0 (VSIEN) is implemented; all other bits are RAZ/WI.
        axi_write(IRQ_EN_ADDR, x"DEADBE01",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        axi_read(IRQ_EN_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(0) = '1', "IRQEn: VSIEN bit set", test_count, pass_count, fail_count);
        check(rdata(31 downto 1) = std_logic_vector(to_unsigned(0,31)),
              "IRQEn: reserved bits [31:1] RAZ", test_count, pass_count, fail_count);
        -- IRQ Clear/Status register: only bit 0 (VSIA) is implemented
        axi_write(IRQ_CLR_ADDR, x"DEADBEEE",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_read(IRQ_CLR_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata(31 downto 1) = std_logic_vector(to_unsigned(0,31)),
              "IRQClr: reserved bits [31:1] RAZ", test_count, pass_count, fail_count);

        -- Test 8.3: Writing 0 to VSIA (W1C) must have no effect on pending flag.
        -- First ensure interrupt is enabled and wait for a VSYNC to set VSIA.
        axi_write(IRQ_EN_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait until rising_edge(vga_vsync);
        wait for 500 ns;

        axi_read(IRQ_CLR_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(0) = '1', "IRQClr: VSIA set after VSYNC", test_count, pass_count, fail_count);

        -- Write VSIA=0 (bit 0 = 0): W1C means writing 0 must NOT clear the flag.
        axi_write(IRQ_CLR_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 100 ns;
        axi_read(IRQ_CLR_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(0) = '1', "IRQClr: VSIA not cleared by writing 0", test_count, pass_count, fail_count);
        check(irq = '1', "IRQ still asserted after W1C=0 write", test_count, pass_count, fail_count);

        -- Now actually clear it (W1C=1 to bit 0 of IRQ_CLR_ADDR).
        axi_write(IRQ_CLR_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 100 ns;
        axi_read(IRQ_CLR_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(0) = '0', "IRQClr: VSIA cleared by W1C=1", test_count, pass_count, fail_count);
        check(irq = '0', "IRQ deasserted after W1C clear", test_count, pass_count, fail_count);

        -- Test 8.4: Interrupt does not fire when VSIEN=0.
        axi_write(IRQ_EN_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait until rising_edge(vga_vsync);
        wait for 500 ns;
        check(irq = '0', "IRQ not asserted when VSIEN=0", test_count, pass_count, fail_count);

        -- VSIA is still set in IRQ_CLR_ADDR even when VSIEN=0.
        axi_read(IRQ_CLR_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(0) = '1', "IRQClr: VSIA set even when VSIEN=0", test_count, pass_count, fail_count);

        ----------------------------------------------------------------------
        -- TEST SUITE 9: Palette Boundary and RMW
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 9: Palette Boundary and RMW ==="));
        writeline(output, l);

        -- Test 9.1: Last palette word (entries 254 and 255) at offset 0x1FC.
        axi_write(std_logic_vector(unsigned(PALETTE_BASE) + 16#1FC#), x"0F0F0A0A",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 50 ns;
        axi_read(std_logic_vector(unsigned(PALETTE_BASE) + 16#1FC#), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(11 downto 0)  = x"A0A", "Palette entry 254 (last even)", test_count, pass_count, fail_count);
        check(rdata(27 downto 16) = x"F0F", "Palette entry 255 (last odd)",  test_count, pass_count, fail_count);

        -- Test 9.2: Palette upper 4 bits of each halfword are RAZ/WI.
        -- Write 0xFFFF_FFFF; upper nibbles of each halfword must read back as 0.
        axi_write(PALETTE_BASE, x"FFFFFFFF",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 50 ns;
        axi_read(PALETTE_BASE, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(15 downto 12) = "0000", "Palette word0: even upper nibble RAZ", test_count, pass_count, fail_count);
        check(rdata(31 downto 28) = "0000", "Palette word0: odd upper nibble RAZ",  test_count, pass_count, fail_count);

        -- Test 9.3: Palette RMW - update only the even entry (low halfword) via
        -- 16-bit write at the word-aligned address, leaving odd entry intact.
        -- First write a known pattern to both entries and verify it was stored.
        axi_write(std_logic_vector(unsigned(PALETTE_BASE) + 8), x"0CCC0AAA",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 50 ns;
        axi_read(std_logic_vector(unsigned(PALETTE_BASE) + 8), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(11 downto 0)  = x"AAA", "Palette RMW setup: even entry", test_count, pass_count, fail_count);
        check(rdata(27 downto 16) = x"CCC", "Palette RMW setup: odd entry",  test_count, pass_count, fail_count);

        -- Now update only entry 4 (even, low halfword) with 0xBBB via 16-bit write.
        -- addr(1:0)="00" -> strobe "0011", wdata(15:0)=0x0BBB, bytes 2-3 preserved by RMW.
        axi_write(std_logic_vector(unsigned(PALETTE_BASE) + 8), x"0BBB",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 50 ns;
        axi_read(std_logic_vector(unsigned(PALETTE_BASE) + 8), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(11 downto 0)  = x"BBB", "Palette RMW: even entry updated",  test_count, pass_count, fail_count);
        check(rdata(27 downto 16) = x"CCC", "Palette RMW: odd entry preserved", test_count, pass_count, fail_count);

        ----------------------------------------------------------------------
        -- TEST SUITE 10: Address Decode Boundaries
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 10: Address Decode Boundaries ==="));
        writeline(output, l);

        -- Test 10.1: Last word of framebuffer.
        -- FB_BRAM_DEPTH = 64,000 words = 256,000 bytes.
        -- Last valid word index: 63,999.  Byte offset: 63999 * 4 = 0x3E7FC.
        -- (Old address 0x3FFFC = word 65535 was valid when depth was 65536;
        --  it is now beyond the array and would cause an out-of-bounds error.)
        axi_write(std_logic_vector(unsigned(FB_BASE) + 16#3E7FC#), x"CAFEBABE",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 50 ns;
        axi_read(std_logic_vector(unsigned(FB_BASE) + 16#3E7FC#), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata = x"CAFEBABE", "FB last word write/read", test_count, pass_count, fail_count);

        -- Test 10.2: First word of palette (0x40000) reads back independently from FB.
        -- Write a different value to FB word at 0x00000 and palette word at 0x40000;
        -- confirm they do not alias.
        axi_write(FB_BASE, x"11111111",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        axi_write(PALETTE_BASE, x"0DEF0ABC",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 50 ns;
        axi_read(FB_BASE, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata = x"11111111", "FB/palette no alias: FB word 0", test_count, pass_count, fail_count);

        axi_read(PALETTE_BASE, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(11 downto 0)  = x"ABC", "FB/palette no alias: palette entry 0", test_count, pass_count, fail_count);

        -- Test 10.3: Last word of palette (0x401FC) does not alias register bank.
        axi_write(std_logic_vector(unsigned(PALETTE_BASE) + 16#1FC#), x"0EEE0DDD",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        axi_write(CTRL_REG_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait for 50 ns;
        axi_read(std_logic_vector(unsigned(PALETTE_BASE) + 16#1FC#), rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(11 downto 0) = x"DDD", "Palette/reg no alias: palette last word", test_count, pass_count, fail_count);

        axi_read(CTRL_REG_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);

        check(rdata(0) = '1', "Palette/reg no alias: ctrl reg intact", test_count, pass_count, fail_count);

        ----------------------------------------------------------------------
        -- TEST SUITE 11: VGA Timing Accuracy
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 11: VGA Timing Accuracy ==="));
        writeline(output, l);

        -- Test 11.1: Measure VSYNC period.
        -- At 25.175 MHz pixel clock: 800 * 449 * 39.7ns = 14.264 ms per frame.
        -- Switch to 640x400 mode first.
        axi_write(CTRL_REG_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait until rising_edge(vga_vsync);
        vsync_time_1 <= now;
        wait until rising_edge(vga_vsync);
        vsync_time_2 <= now;
        wait for 0 ns;  -- let signals settle

        check(vsync_time_2 - vsync_time_1 > 13 ms, "VSYNC period > 13ms",  test_count, pass_count, fail_count);
        check(vsync_time_2 - vsync_time_1 < 16 ms, "VSYNC period < 16ms",  test_count, pass_count, fail_count);

        -- Test 11.2: HSYNC period (800 pixels * 39.7ns = 31.76us).
        wait until falling_edge(vga_hsync);
        hsync_time_1 <= now;
        wait until falling_edge(vga_hsync);
        hsync_time_2 <= now;
        wait for 0 ns;

        check(hsync_time_2 - hsync_time_1 > 30 us, "HSYNC period > 30us", test_count, pass_count, fail_count);
        check(hsync_time_2 - hsync_time_1 < 34 us, "HSYNC period < 34us", test_count, pass_count, fail_count);

        -- Test 11.3: video_on is low during blanking (immediately after VSYNC rising edge).
        -- VSYNC rises at v_count=412; active region is v_count < 400, so we are
        -- in blanking for the next 37 lines (1.175 ms).
        wait until rising_edge(vga_vsync);
        wait for 100 ns;
        check(vga_video_on = '0', "video_on low during vertical blanking", test_count, pass_count, fail_count);

        -- Test 11.4: video_on is high during active video.
        -- Active video resumes 37 lines (1.175 ms) after VSYNC rising edge and
        -- lasts 400 lines (12.704 ms).  Wait 5 ms: well past the 1.175 ms blanking
        -- but comfortably before the 13.879 ms end of active region.
        wait until rising_edge(vga_vsync);
        wait for 5 ms;
        check(vga_video_on = '1', "video_on high during active video", test_count, pass_count, fail_count);

        -- Test 11.5: Pixel data is non-zero during active video after loading a palette.
        -- Entry 0 is now 0xFFF (written in suite 9.2). If any pixel has colour index 0
        -- it will output 0xFFF (white). Load a known non-zero entry for index 0.
        axi_write(PALETTE_BASE, x"00000FFF",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait until rising_edge(vga_vsync);
        wait for 2 ms;  -- active region

        ----------------------------------------------------------------------
        ----------------------------------------------------------------------
        -- TEST SUITE 12: Burst Read/Write
        -- Tests AXI4 INCR bursts to the framebuffer and palette regions.
        -- Framebuffer: up to 256 beats of 4 bytes each (1 KB per burst).
        -- Palette:     up to 128 beats covering the full 128-word array.
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 12: Burst Read/Write ==="));
        writeline(output, l);

        -- ------------------------------------------------------------------
        -- Test 12.1: 4-beat burst write then burst read back — framebuffer
        -- Write words 0xAABBCC00..0xAABBCC03 to FB_BASE+0x1000.
        -- ------------------------------------------------------------------
        burst_wr_data(0) <= x"AABBCC00";
        burst_wr_data(1) <= x"AABBCC01";
        burst_wr_data(2) <= x"AABBCC02";
        burst_wr_data(3) <= x"AABBCC03";
        wait for 0 ns;

        axi_burst_write(std_logic_vector(unsigned(FB_BASE) + 16#1000#),
                        burst_wr_data(0 to 3), 4,
                        s_axi_awaddr, s_axi_awlen, s_axi_awburst,
                        s_axi_awvalid, s_axi_awready,
                        s_axi_wdata, s_axi_wstrb, s_axi_wlast,
                        s_axi_wvalid, s_axi_wready,
                        s_axi_bvalid, s_axi_bready, axi_clk);

        wait for 100 ns;

        axi_burst_read(std_logic_vector(unsigned(FB_BASE) + 16#1000#), 4,
                       burst_rd_data(0 to 3),
                       s_axi_araddr, s_axi_arlen, s_axi_arburst,
                       s_axi_arvalid, s_axi_arready,
                       s_axi_rdata, s_axi_rlast, s_axi_rvalid, s_axi_rready,
                       axi_clk);

        wait for 0 ns;
        check(burst_rd_data(0) = x"AABBCC00", "FB burst: beat 0",
              test_count, pass_count, fail_count);
        check(burst_rd_data(1) = x"AABBCC01", "FB burst: beat 1",
              test_count, pass_count, fail_count);
        check(burst_rd_data(2) = x"AABBCC02", "FB burst: beat 2",
              test_count, pass_count, fail_count);
        check(burst_rd_data(3) = x"AABBCC03", "FB burst: beat 3",
              test_count, pass_count, fail_count);

        -- ------------------------------------------------------------------
        -- Test 12.2: 8-beat burst write then burst read back — framebuffer
        -- Verifies a longer burst and that addresses increment correctly.
        -- Write 0xDEAD_00xx to FB_BASE+0x2000.
        -- ------------------------------------------------------------------
        burst_wr_data(0) <= x"DEAD0000";
        burst_wr_data(1) <= x"DEAD0001";
        burst_wr_data(2) <= x"DEAD0002";
        burst_wr_data(3) <= x"DEAD0003";
        burst_wr_data(4) <= x"DEAD0004";
        burst_wr_data(5) <= x"DEAD0005";
        burst_wr_data(6) <= x"DEAD0006";
        burst_wr_data(7) <= x"DEAD0007";
        wait for 0 ns;

        axi_burst_write(std_logic_vector(unsigned(FB_BASE) + 16#2000#),
                        burst_wr_data(0 to 7), 8,
                        s_axi_awaddr, s_axi_awlen, s_axi_awburst,
                        s_axi_awvalid, s_axi_awready,
                        s_axi_wdata, s_axi_wstrb, s_axi_wlast,
                        s_axi_wvalid, s_axi_wready,
                        s_axi_bvalid, s_axi_bready, axi_clk);

        wait for 100 ns;

        axi_burst_read(std_logic_vector(unsigned(FB_BASE) + 16#2000#), 8,
                       burst_rd_data(0 to 7),
                       s_axi_araddr, s_axi_arlen, s_axi_arburst,
                       s_axi_arvalid, s_axi_arready,
                       s_axi_rdata, s_axi_rlast, s_axi_rvalid, s_axi_rready,
                       axi_clk);

        wait for 0 ns;
        check(burst_rd_data(0) = x"DEAD0000", "FB 8-beat burst: beat 0",
              test_count, pass_count, fail_count);
        check(burst_rd_data(3) = x"DEAD0003", "FB 8-beat burst: beat 3",
              test_count, pass_count, fail_count);
        check(burst_rd_data(7) = x"DEAD0007", "FB 8-beat burst: beat 7",
              test_count, pass_count, fail_count);

        -- ------------------------------------------------------------------
        -- Test 12.3: Burst write does not corrupt adjacent words.
        -- Write a sentinel word before and after the burst region.
        -- Burst-write 4 words; read back sentinels and verify they are intact.
        -- ------------------------------------------------------------------
        -- Write sentinels.
        axi_write(std_logic_vector(unsigned(FB_BASE) + 16#3000# - 4), x"CAFEBABE",
                  s_axi_awaddr, s_axi_awvalid, s_axi_awready,
                  s_axi_wdata, s_axi_wstrb, s_axi_wvalid, s_axi_wready,
                  s_axi_bvalid, s_axi_bready, axi_clk);

        axi_write(std_logic_vector(unsigned(FB_BASE) + 16#3000# + 16), x"DEADF00D",
                  s_axi_awaddr, s_axi_awvalid, s_axi_awready,
                  s_axi_wdata, s_axi_wstrb, s_axi_wvalid, s_axi_wready,
                  s_axi_bvalid, s_axi_bready, axi_clk);

        -- Burst-write 4 words into the middle region.
        burst_wr_data(0) <= x"11223344";
        burst_wr_data(1) <= x"55667788";
        burst_wr_data(2) <= x"99AABBCC";
        burst_wr_data(3) <= x"DDEEFF00";
        wait for 0 ns;

        axi_burst_write(std_logic_vector(unsigned(FB_BASE) + 16#3000#),
                        burst_wr_data(0 to 3), 4,
                        s_axi_awaddr, s_axi_awlen, s_axi_awburst,
                        s_axi_awvalid, s_axi_awready,
                        s_axi_wdata, s_axi_wstrb, s_axi_wlast,
                        s_axi_wvalid, s_axi_wready,
                        s_axi_bvalid, s_axi_bready, axi_clk);

        wait for 50 ns;

        -- Read back sentinels.
        axi_read(std_logic_vector(unsigned(FB_BASE) + 16#3000# - 4), rdata,
                 s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata = x"CAFEBABE", "FB burst: lower sentinel intact",
              test_count, pass_count, fail_count);

        axi_read(std_logic_vector(unsigned(FB_BASE) + 16#3000# + 16), rdata,
                 s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata = x"DEADF00D", "FB burst: upper sentinel intact",
              test_count, pass_count, fail_count);

        -- ------------------------------------------------------------------
        -- Test 12.4: 4-beat burst write then burst read back — palette
        -- Write palette words 8..11 (entries 16..23).
        -- Each 32-bit word holds two 12-bit RGB entries; upper nibble masked.
        -- ------------------------------------------------------------------
        -- Data: odd entry in [31:16], even entry in [15:0]; upper nibbles must
        -- be zero after the mask-on-write in palette_bram.
        burst_wr_data(0) <= x"0F0F0E0E";  -- entry 16 = 0xE0E, entry 17 = 0xF0F
        burst_wr_data(1) <= x"0D0D0C0C";  -- entry 18 = 0xC0C, entry 19 = 0xD0D
        burst_wr_data(2) <= x"0B0B0A0A";  -- entry 20 = 0xA0A, entry 21 = 0xB0B
        burst_wr_data(3) <= x"09090808";  -- entry 22 = 0x808, entry 23 = 0x909
        wait for 0 ns;

        axi_burst_write(std_logic_vector(unsigned(PALETTE_BASE) + 8*4),
                        burst_wr_data(0 to 3), 4,
                        s_axi_awaddr, s_axi_awlen, s_axi_awburst,
                        s_axi_awvalid, s_axi_awready,
                        s_axi_wdata, s_axi_wstrb, s_axi_wlast,
                        s_axi_wvalid, s_axi_wready,
                        s_axi_bvalid, s_axi_bready, axi_clk);

        wait for 100 ns;

        axi_burst_read(std_logic_vector(unsigned(PALETTE_BASE) + 8*4), 4,
                       burst_rd_data(0 to 3),
                       s_axi_araddr, s_axi_arlen, s_axi_arburst,
                       s_axi_arvalid, s_axi_arready,
                       s_axi_rdata, s_axi_rlast, s_axi_rvalid, s_axi_rready,
                       axi_clk);

        wait for 0 ns;
        -- Verify even and odd entries; upper nibbles must be RAZ.
        check(burst_rd_data(0)(11 downto 0)  = x"E0E",
              "Palette burst: word 8 even entry (16)", test_count, pass_count, fail_count);
        check(burst_rd_data(0)(27 downto 16) = x"F0F",
              "Palette burst: word 8 odd entry (17)",  test_count, pass_count, fail_count);
        check(burst_rd_data(0)(15 downto 12) = "0000",
              "Palette burst: word 8 even upper nibble RAZ", test_count, pass_count, fail_count);
        check(burst_rd_data(0)(31 downto 28) = "0000",
              "Palette burst: word 8 odd upper nibble RAZ",  test_count, pass_count, fail_count);
        check(burst_rd_data(3)(11 downto 0)  = x"808",
              "Palette burst: word 11 even entry (22)", test_count, pass_count, fail_count);
        check(burst_rd_data(3)(27 downto 16) = x"909",
              "Palette burst: word 11 odd entry (23)",  test_count, pass_count, fail_count);

        -- ------------------------------------------------------------------
        -- Test 12.5: Burst write/read of the last 4 words of the framebuffer.
        -- FB_BRAM_DEPTH = 64,000 words; last valid word index = 63,999.
        -- Last 4 words: indices 63996..63999, byte offsets 0x3E7F0..0x3E7FC.
        -- (Old address 0x3FFF0 = word 65532 was valid when depth was 65,536;
        --  all four beats are now beyond the array and cause out-of-bounds errors.)
        -- ------------------------------------------------------------------
        burst_wr_data(0) <= x"FACE0000";
        burst_wr_data(1) <= x"FACE0001";
        burst_wr_data(2) <= x"FACE0002";
        burst_wr_data(3) <= x"FACE0003";
        wait for 0 ns;

        axi_burst_write(std_logic_vector(unsigned(FB_BASE) + 16#3E7F0#),
                        burst_wr_data(0 to 3), 4,
                        s_axi_awaddr, s_axi_awlen, s_axi_awburst,
                        s_axi_awvalid, s_axi_awready,
                        s_axi_wdata, s_axi_wstrb, s_axi_wlast,
                        s_axi_wvalid, s_axi_wready,
                        s_axi_bvalid, s_axi_bready, axi_clk);

        wait for 100 ns;

        axi_burst_read(std_logic_vector(unsigned(FB_BASE) + 16#3E7F0#), 4,
                       burst_rd_data(0 to 3),
                       s_axi_araddr, s_axi_arlen, s_axi_arburst,
                       s_axi_arvalid, s_axi_arready,
                       s_axi_rdata, s_axi_rlast, s_axi_rvalid, s_axi_rready,
                       axi_clk);

        wait for 0 ns;
        check(burst_rd_data(0) = x"FACE0000", "FB end burst: beat 0",
              test_count, pass_count, fail_count);
        check(burst_rd_data(3) = x"FACE0003", "FB end burst: beat 3",
              test_count, pass_count, fail_count);

        ----------------------------------------------------------------------
        -- TEST SUITE 13: VSYNC Interrupt Frequency Measurement
        --
        -- Method: enable the VSYNC interrupt, then capture the simulation
        -- timestamp on three consecutive rising edges of the irq signal.
        -- Two consecutive periods are measured and reported.  The expected
        -- period is 800 * 449 * 39.7 ns = 14.264 ms (70.10 Hz).
        --
        -- The test checks:
        --   1. IRQ fires at all (not stuck low).
        --   2. Period is within ±5 % of the theoretical value.
        --   3. Both measured periods are consistent (jitter < 1 us).
        --
        -- After measurement the interrupt is disabled and the IRQ line is
        -- verified to go low, confirming the enable gate works correctly.
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 13: VSYNC Interrupt Frequency ==="));
        writeline(output, l);

        -- Ensure we are in 640x400 mode for a deterministic pixel clock ratio.
        axi_write(CTRL_REG_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- Clear any pending interrupt left over from earlier suites.
        axi_write(IRQ_CLR_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- Enable VSYNC interrupt.
        axi_write(IRQ_EN_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- Capture three consecutive IRQ rising edges.
        wait until rising_edge(irq);
        irq_time_1 <= now;

        -- Clear the interrupt so it can fire again on the next VSYNC.
        axi_write(IRQ_CLR_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(IRQ_EN_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait until rising_edge(irq);
        irq_time_2 <= now;

        -- Clear and re-enable for the third edge.
        axi_write(IRQ_CLR_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(IRQ_EN_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        wait until rising_edge(irq);
        irq_time_3 <= now;

        -- Let signal assignments propagate.
        wait for 0 ns;

        -- Compute periods.
        irq_period_1 <= irq_time_2 - irq_time_1;
        irq_period_2 <= irq_time_3 - irq_time_2;
        wait for 0 ns;

        -- Report results.
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("--- VSYNC IRQ period measurement ---"));
        writeline(output, l);
        write(l, string'("  Period 1 (ns): ") & time'image(irq_period_1));
        writeline(output, l);
        write(l, string'("  Period 2 (ns): ") & time'image(irq_period_2));
        writeline(output, l);
        -- Theoretical: 800 * 449 * 39.7 ns = 14,264,360 ns  => ~70.10 Hz
        -- 5% tolerance band: 13,551,142 ns .. 14,977,578 ns
        check(irq_period_1 > 13500 us, "IRQ period 1 > 13.5 ms (lower bound)",
              test_count, pass_count, fail_count);
        check(irq_period_1 < 15000 us, "IRQ period 1 < 15.0 ms (upper bound)",
              test_count, pass_count, fail_count);
        check(irq_period_2 > 13500 us, "IRQ period 2 > 13.5 ms (lower bound)",
              test_count, pass_count, fail_count);
        check(irq_period_2 < 15000 us, "IRQ period 2 < 15.0 ms (upper bound)",
              test_count, pass_count, fail_count);

        -- Jitter: the two periods should differ by less than 1 us.
        -- Use abs() via conditional expression (VHDL-93 compatible).
        if irq_period_1 > irq_period_2 then
            check(irq_period_1 - irq_period_2 < 1 us, "IRQ period jitter < 1 us",
                  test_count, pass_count, fail_count);
        else
            check(irq_period_2 - irq_period_1 < 1 us, "IRQ period jitter < 1 us",
                  test_count, pass_count, fail_count);
        end if;

        -- Disable interrupt and verify IRQ goes low.
        axi_write(IRQ_EN_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        wait for 100 ns;
        check(irq = '0', "IRQ low after VSIEN disabled",
              test_count, pass_count, fail_count);

        ----------------------------------------------------------------------
        -- TEST SUITE 14: Reset Behaviour
        -- Verifies that asserting aresetn=0 clears all registers and drops
        -- the IRQ line, and that the design resumes correctly afterwards.
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 14: Reset Behaviour ==="));
        writeline(output, l);

        -- 14.1: Set up known state before reset.
        axi_write(CTRL_REG_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(DISP_BUF_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(IRQ_EN_ADDR,   x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        -- Wait for a VSYNC so VSIA gets set, then verify IRQ is asserted.
        wait until rising_edge(vga_vsync);
        wait for 500 ns;
        check(irq = '1', "Reset setup: IRQ asserted before reset",
              test_count, pass_count, fail_count);

        -- 14.2: Assert reset.  All registers and the IRQ output should clear
        --        synchronously within a few clock cycles.
        resetn <= '0';
        wait for 5 * CLK_PERIOD_AXI;
        check(irq = '0', "Reset: IRQ deasserted during reset",
              test_count, pass_count, fail_count);

        -- 14.3: Release reset and verify registers read back as zero.
        resetn <= '1';
        wait for 10 * CLK_PERIOD_AXI;

        axi_read(CTRL_REG_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata(0) = '0', "Reset: MODE cleared after reset",
              test_count, pass_count, fail_count);

        axi_read(DISP_BUF_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata(0) = '0', "Reset: BUF cleared after reset",
              test_count, pass_count, fail_count);

        axi_read(IRQ_EN_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata(0) = '0', "Reset: VSIEN cleared after reset",
              test_count, pass_count, fail_count);

        axi_read(IRQ_CLR_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata(0) = '0', "Reset: VSIA cleared after reset",
              test_count, pass_count, fail_count);

        -- 14.4: Verify normal operation resumes after reset.
        --       Write and read back a framebuffer word.
        axi_write(FB_BASE, x"DEADBEEF",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        wait for 50 ns;
        axi_read(FB_BASE, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata = x"DEADBEEF", "Reset: FB write/read works after reset",
              test_count, pass_count, fail_count);

        -- 14.5: Re-enable interrupt and verify it fires again after reset.
        axi_write(IRQ_EN_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        wait until rising_edge(vga_vsync);
        wait for 500 ns;
        check(irq = '1', "Reset: IRQ fires again after reset release",
              test_count, pass_count, fail_count);
        -- Leave interrupts disabled for subsequent suites.
        axi_write(IRQ_EN_ADDR,   x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(IRQ_CLR_ADDR,  x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        ----------------------------------------------------------------------
        -- TEST SUITE 15: Pixel Pipeline RGB Output
        -- Writes a known pixel index to the framebuffer, loads a known colour
        -- into the matching palette entry, then waits for the active-video
        -- period and checks that red/green/blue match the palette entry.
        -- Uses 640x400 mode (1:1 pixel mapping) so fb_addr = y*640+x.
        -- Checks the pixel at screen position (0,0): fb_addr = 0.
        -- Palette entry 0 (even half of word 0) is in bits [11:0].
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 15: Pixel Pipeline RGB Output ==="));
        writeline(output, l);

        -- 15.1: Select 640x400 mode, buffer 0.
        axi_write(CTRL_REG_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(DISP_BUF_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- 15.2: Write a known 12-bit colour (0xA5C) for palette entry 5.
        --       Entry 5 is the odd half of palette word 2 (byte offset 8),
        --       stored in bits [27:16].  Entry 4 is in bits [11:0].
        --       Write both halves together: entry 4 = 0x000, entry 5 = 0x0A5C.
        axi_write(std_logic_vector(unsigned(PALETTE_BASE) + 8), x"0A5C0000",
                  s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- 15.3: Write pixel index 0x05 to all four bytes of framebuffer word 0.
        --       Using a full-word write ensures that regardless of which byte
        --       the pipeline samples first (due to look-ahead offset), all
        --       pixels at positions 0-3 map to palette entry 5.
        axi_write(FB_BASE, x"05050505",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- 15.3b: Read back palette word 2 and FB word 0 to confirm writes landed.
        axi_read(std_logic_vector(unsigned(PALETTE_BASE) + 8), rdata,
                 s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        report "DIAG suite15: palette word2 readback = " &
               integer'image(to_integer(unsigned(rdata))) &
               "  (expect 174522368)";
        axi_read(FB_BASE, rdata,
                 s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        report "DIAG suite15: FB word0 byte0 = " &
               integer'image(to_integer(unsigned(rdata(7 downto 0)))) &
               "  (expect 5, all bytes should be 5)";

        -- 15.4: Wait for the start of active video then sample pixel (0,0).
        --       After rising_edge(video_on), each pixel_clk edge advances the
        --       displayed pixel by one position.  Pixel 0 is valid 1 edge after
        --       video_on rises (BRAM latency already covered by look-ahead;
        --       one more edge for the palette registered output).
        wait until rising_edge(vga_video_on);
        wait until rising_edge(pixel_clk);  -- pixel 0 RGB now valid

        -- 15.5: Check R/G/B outputs match palette entry 5 = 0xA5C.
        --       R = 0xA, G = 0x5, B = 0xC.
        report "DIAG suite15: red="   & integer'image(to_integer(unsigned(red)))   &
               " green=" & integer'image(to_integer(unsigned(green))) &
               " blue="  & integer'image(to_integer(unsigned(blue)))  &
               " video_on=" & std_logic'image(vga_video_on);
        check(red   = x"A", "Pipeline RGB: red   = 0xA for palette entry 5",
              test_count, pass_count, fail_count);
        check(green = x"5", "Pipeline RGB: green = 0x5 for palette entry 5",
              test_count, pass_count, fail_count);
        check(blue  = x"C", "Pipeline RGB: blue  = 0xC for palette entry 5",
              test_count, pass_count, fail_count);

        -- 15.6: Verify RGB is forced to zero during blanking (video_on=0).
        wait until rising_edge(vga_vsync);
        wait for 100 ns;  -- still in vertical blanking
        check(red   = x"0", "Pipeline RGB: red   = 0 during blanking",
              test_count, pass_count, fail_count);
        check(green = x"0", "Pipeline RGB: green = 0 during blanking",
              test_count, pass_count, fail_count);
        check(blue  = x"0", "Pipeline RGB: blue  = 0 during blanking",
              test_count, pass_count, fail_count);

        ----------------------------------------------------------------------
        -- TEST SUITE 16: Mode Switch Affects Framebuffer Address Calculation
        -- Verifies that switching MODE changes which framebuffer region is
        -- displayed, not just the register bit.  In 320x200 mode fb_addr uses
        -- halved x/y coordinates; in 640x400 mode it uses 1:1 coordinates.
        -- The test writes distinguishable patterns to the two address regions
        -- and checks that the correct one reaches the pipeline outputs.
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 16: Mode Switch Affects FB Address ==="));
        writeline(output, l);

        -- 16.1: In 640x400 mode pixel (0,0) maps to fb_addr=0.
        --       Write index 0x01 to address 0.
        axi_write(CTRL_REG_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(FB_BASE, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        -- Load palette entry 1 = 0x111 (R=1,G=1,B=1).
        -- Entry 1 is the odd half of palette word 0, bits [27:16].
        axi_write(PALETTE_BASE, x"01110000",
                  s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- 16.2: In 320x200 mode pixel (0,0) also maps to fb_addr=0 (halved
        --       coords of 0 are still 0), so we instead check a non-zero
        --       position.  Pixel (2,0) in 640x400 uses fb_addr=2; in 320x200
        --       pixel (2,0) halves to (1,0) and uses fb_addr=1.
        --       Each pixel index occupies one byte; byte N is at word-aligned
        --       address (N/4)*4 in byte lane (N mod 4).
        --       fb_addr=1 -> byte 1 of word 0 (wdata byte lane 1, addr+0).
        --       fb_addr=2 -> byte 2 of word 0 (wdata byte lane 2, addr+0).
        --       Write both in one word: byte0=0x01, byte1=0x02, byte2=0x07.
        axi_write(FB_BASE, x"00070201",
                  s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        -- Palette entry 2 = 0x222, entry 7 = 0x777.
        -- Entries 2,3 are in palette word 1 (byte offset 4): bits[11:0]=entry2, bits[27:16]=entry3.
        -- Entries 6,7 are in palette word 3 (byte offset 12): bits[11:0]=entry6, bits[27:16]=entry7.
        axi_write(std_logic_vector(unsigned(PALETTE_BASE) + 4), x"00000222",
                  s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(std_logic_vector(unsigned(PALETTE_BASE) + 12), x"07770000",
                  s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- 16.3: In 640x400 mode check pixel (2,0) gives index 7 -> 0x777.
        --       Pixel N is valid N+1 pixel_clk edges after rising_edge(video_on).
        axi_write(CTRL_REG_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        wait until rising_edge(vga_vsync);
        wait until rising_edge(vga_video_on);
        wait until rising_edge(pixel_clk);  -- pixel 0
        wait until rising_edge(pixel_clk);  -- pixel 1
        wait until rising_edge(pixel_clk);  -- pixel 2 RGB now valid
        check(red   = x"7", "Mode 640x400: pixel(2,0) R=7",
              test_count, pass_count, fail_count);
        check(green = x"7", "Mode 640x400: pixel(2,0) G=7",
              test_count, pass_count, fail_count);
        check(blue  = x"7", "Mode 640x400: pixel(2,0) B=7",
              test_count, pass_count, fail_count);

        -- 16.4: Switch to 320x200 mode; pixel (2,0) now halves to (1,0)
        --       which uses fb_addr=1, index 2 -> palette 0x222.
        axi_write(CTRL_REG_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        wait until rising_edge(vga_vsync);
        wait until rising_edge(vga_video_on);
        wait until rising_edge(pixel_clk);  -- pixel 0
        wait until rising_edge(pixel_clk);  -- pixel 1
        wait until rising_edge(pixel_clk);  -- pixel 2 RGB now valid
        check(red   = x"2", "Mode 320x200: pixel(2,0) R=2 (halved addr)",
              test_count, pass_count, fail_count);
        check(green = x"2", "Mode 320x200: pixel(2,0) G=2 (halved addr)",
              test_count, pass_count, fail_count);
        check(blue  = x"2", "Mode 320x200: pixel(2,0) B=2 (halved addr)",
              test_count, pass_count, fail_count);

        -- Restore 640x400 for subsequent suites.
        axi_write(CTRL_REG_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        ----------------------------------------------------------------------
        -- TEST SUITE 17: Double-Buffer Switch Takes Effect on Frame Boundary
        -- Double buffering is only available in 320x200 mode (two 64,000-byte
        -- buffers fit within the 256,000-byte BRAM).  640x400 mode uses the
        -- full BRAM as a single buffer.
        -- Writes distinguishable pixel indices to buffer 0 (base=0x00000) and
        -- buffer 1 (base=FB_BUFFER_1_BASE), loads matching palette entries,
        -- then confirms the displayed colour changes when DISP_BUF is toggled.
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 17: Double Buffer Display Switch ==="));
        writeline(output, l);

        -- 17.1: Switch to 320x200 mode for double-buffer test.
        axi_write(CTRL_REG_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- 17.2: Write index 0x0A to all bytes of pixel word 0 in buffer 0,
        --       index 0x0B to all bytes of pixel word 0 in buffer 1.
        axi_write(FB_BASE, x"0A0A0A0A",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(FB_BUFFER_1_BASE, x"0B0B0B0B",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        -- Palette entry 0x0A = 0xAAA, 0x0B = 0xBBB.
        -- Entries 10 and 11 are in palette word 5 (byte offset 20):
        --   bits[11:0] = entry 10 = 0xAAA, bits[27:16] = entry 11 = 0xBBB.
        axi_write(std_logic_vector(unsigned(PALETTE_BASE) + 20), x"0BBB0AAA",
                  s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- 17.3: Select buffer 0 and verify display shows 0xAAA.
        axi_write(DISP_BUF_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        wait until rising_edge(vga_vsync);
        wait until rising_edge(vga_video_on);
        wait until rising_edge(pixel_clk);  -- pixel 0 RGB now valid
        check(red   = x"A", "DblBuf: buf0 R=A",
              test_count, pass_count, fail_count);
        check(green = x"A", "DblBuf: buf0 G=A",
              test_count, pass_count, fail_count);
        check(blue  = x"A", "DblBuf: buf0 B=A",
              test_count, pass_count, fail_count);

        -- 17.4: Switch to buffer 1 and verify display shows 0xBBB.
        axi_write(DISP_BUF_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        wait until rising_edge(vga_vsync);
        wait until rising_edge(vga_video_on);
        wait until rising_edge(pixel_clk);  -- pixel 0 RGB now valid
        check(red   = x"B", "DblBuf: buf1 R=B",
              test_count, pass_count, fail_count);
        check(green = x"B", "DblBuf: buf1 G=B",
              test_count, pass_count, fail_count);
        check(blue  = x"B", "DblBuf: buf1 B=B",
              test_count, pass_count, fail_count);

        -- Restore 640x400, buffer 0 for subsequent suites.
        axi_write(CTRL_REG_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(DISP_BUF_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        ----------------------------------------------------------------------
        -- TEST SUITE 18: Arbiter Read/Write Contention
        -- Issues a burst AXI write to the framebuffer while the pixel pipeline
        -- is actively reading port B, then reads back the written values to
        -- confirm neither path corrupted the other.
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 18: Arbiter Read/Write Contention ==="));
        writeline(output, l);

        -- 18.1: Set up 640x400 mode so the pipeline is reading continuously.
        axi_write(CTRL_REG_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- 18.2: Wait until the pipeline is in the middle of the active region,
        --       then immediately issue a burst write to a different FB region.
        wait until rising_edge(vga_video_on);
        wait for 500 ns;  -- pipeline actively reading

        burst_wr_data(0) <= x"C0FFEE00";
        burst_wr_data(1) <= x"C0FFEE01";
        burst_wr_data(2) <= x"C0FFEE02";
        burst_wr_data(3) <= x"C0FFEE03";
        wait for 0 ns;

        axi_burst_write(std_logic_vector(unsigned(FB_BASE) + 16#8000#),
                        burst_wr_data(0 to 3), 4,
                        s_axi_awaddr, s_axi_awlen, s_axi_awburst,
                        s_axi_awvalid, s_axi_awready,
                        s_axi_wdata, s_axi_wstrb, s_axi_wlast,
                        s_axi_wvalid, s_axi_wready,
                        s_axi_bvalid, s_axi_bready, axi_clk);

        -- 18.3: Read back immediately to confirm write completed correctly
        --       despite concurrent display reads.
        wait for 100 ns;

        axi_burst_read(std_logic_vector(unsigned(FB_BASE) + 16#8000#), 4,
                       burst_rd_data(0 to 3),
                       s_axi_araddr, s_axi_arlen, s_axi_arburst,
                       s_axi_arvalid, s_axi_arready,
                       s_axi_rdata, s_axi_rlast, s_axi_rvalid, s_axi_rready,
                       axi_clk);
        wait for 0 ns;

        check(burst_rd_data(0) = x"C0FFEE00", "Contention: beat 0 intact",
              test_count, pass_count, fail_count);
        check(burst_rd_data(1) = x"C0FFEE01", "Contention: beat 1 intact",
              test_count, pass_count, fail_count);
        check(burst_rd_data(2) = x"C0FFEE02", "Contention: beat 2 intact",
              test_count, pass_count, fail_count);
        check(burst_rd_data(3) = x"C0FFEE03", "Contention: beat 3 intact",
              test_count, pass_count, fail_count);

        -- 18.4: Also confirm the pixel pipeline is still producing output after
        --       the contention period (video_on still transitions).
        wait until rising_edge(vga_vsync);
        wait until rising_edge(vga_video_on);
        wait for CLK_PERIOD_PIXEL * 3;
        check(vga_video_on = '1', "Contention: pipeline still active after arbiter stress",
              test_count, pass_count, fail_count);

        ----------------------------------------------------------------------
        -- TEST SUITE 19: IRQ Pending on Late VSIEN Enable
        -- If VSIA is already set when software writes VSIEN=1, the IRQ output
        -- should assert on the very same (or next) clock cycle without waiting
        -- for another VSYNC edge.
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 19: IRQ Pending on Late VSIEN Enable ==="));
        writeline(output, l);

        -- 19.1: Ensure VSIEN=0 and clear any pending flag.
        axi_write(IRQ_EN_ADDR,  x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(IRQ_CLR_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        wait for 200 ns;
        check(irq = '0', "Late VSIEN: IRQ low with VSIEN=0",
              test_count, pass_count, fail_count);

        -- 19.2: Wait for a VSYNC to set VSIA without VSIEN enabled.
        wait until rising_edge(vga_vsync);
        wait for 500 ns;
        -- Confirm VSIA is set but IRQ is still low.
        axi_read(IRQ_CLR_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata(0) = '1', "Late VSIEN: VSIA set while VSIEN=0",
              test_count, pass_count, fail_count);
        check(irq = '0', "Late VSIEN: IRQ still low before enable",
              test_count, pass_count, fail_count);

        -- 19.3: Now enable VSIEN.  IRQ should assert immediately (combinational
        --       AND of enable and pending_flag).
        axi_write(IRQ_EN_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        wait for 2 * CLK_PERIOD_AXI;
        check(irq = '1', "Late VSIEN: IRQ asserts immediately on enable with VSIA set",
              test_count, pass_count, fail_count);

        -- 19.4: Clear and disable for subsequent suites.
        axi_write(IRQ_CLR_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(IRQ_EN_ADDR,  x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        ----------------------------------------------------------------------
        -- TEST SUITE 20: WSTRB=0 Write is a No-Op
        -- A write transaction where all byte strobes are zero must not change
        -- any register or memory location.
        -- The axi_write helper infers wstrb from non-zero bytes of wdata, so
        -- we cannot use it here.  Instead drive the AXI signals directly to
        -- issue a write with wdata=0xFFFFFFFF and wstrb=0x0.
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 20: WSTRB=0 Write is a No-Op ==="));
        writeline(output, l);

        -- 20.1: Write a known sentinel to the control register.
        axi_write(CTRL_REG_ADDR, x"00000001",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_read(CTRL_REG_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata(0) = '1', "Wstrb0: sentinel written to CTRL",
              test_count, pass_count, fail_count);

        -- 20.2: Issue a raw AXI write to CTRL_REG_ADDR with wstrb=0x0.
        --       Address phase.
        wait until rising_edge(axi_clk);
        s_axi_awaddr  <= CTRL_REG_ADDR;
        s_axi_awvalid <= '1';
        wait until s_axi_awready = '1' and rising_edge(axi_clk);
        s_axi_awvalid <= '0';

        -- Data phase with all-ones data but zero strobe.
        s_axi_wdata  <= x"FFFFFFFF";
        s_axi_wstrb  <= x"0";
        s_axi_wvalid <= '1';
        s_axi_wlast  <= '1';
        wait until s_axi_wready = '1' and rising_edge(axi_clk);
        s_axi_wvalid <= '0';
        s_axi_wstrb  <= x"0";

        -- Response phase.
        s_axi_bready <= '1';
        wait until s_axi_bvalid = '1' and rising_edge(axi_clk);
        s_axi_bready <= '0';
        wait for CLK_PERIOD_AXI;

        -- Restore wlast to default.
        s_axi_wlast <= '1';

        -- 20.3: Read back; sentinel must be unchanged.
        axi_read(CTRL_REG_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata(0) = '1', "Wstrb0: CTRL register unchanged after wstrb=0 write",
              test_count, pass_count, fail_count);

        -- 20.4: Repeat for a framebuffer word.
        axi_write(std_logic_vector(unsigned(FB_BASE) + 16#100#), x"A5A5A5A5",
                  s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        wait for 50 ns;

        wait until rising_edge(axi_clk);
        s_axi_awaddr  <= std_logic_vector(unsigned(FB_BASE) + 16#100#);
        s_axi_awvalid <= '1';
        wait until s_axi_awready = '1' and rising_edge(axi_clk);
        s_axi_awvalid <= '0';
        s_axi_wdata  <= x"FFFFFFFF";
        s_axi_wstrb  <= x"0";
        s_axi_wvalid <= '1';
        s_axi_wlast  <= '1';
        wait until s_axi_wready = '1' and rising_edge(axi_clk);
        s_axi_wvalid <= '0';
        s_axi_wstrb  <= x"0";
        s_axi_bready <= '1';
        wait until s_axi_bvalid = '1' and rising_edge(axi_clk);
        s_axi_bready <= '0';
        s_axi_wlast  <= '1';
        wait for CLK_PERIOD_AXI;

        axi_read(std_logic_vector(unsigned(FB_BASE) + 16#100#), rdata,
                 s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata = x"A5A5A5A5", "Wstrb0: FB word unchanged after wstrb=0 write",
              test_count, pass_count, fail_count);

        ----------------------------------------------------------------------
        -- TEST SUITE 21: Back-to-Back Writes Without Wait States
        -- Issues several single-word writes on consecutive AXI clock cycles
        -- to stress the AXI ready/valid handshake and verify all writes land.
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 21: Back-to-Back Writes ==="));
        writeline(output, l);

        -- 21.1: Issue four consecutive word writes to adjacent framebuffer
        --       words starting at FB_BASE+0x500.  The axi_write helper waits
        --       for the response before returning, so these are pipelined as
        --       tightly as the handshake allows.
        axi_write(std_logic_vector(unsigned(FB_BASE) + 16#500#), x"11111111",
                  s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(std_logic_vector(unsigned(FB_BASE) + 16#504#), x"22222222",
                  s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(std_logic_vector(unsigned(FB_BASE) + 16#508#), x"33333333",
                  s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);
        axi_write(std_logic_vector(unsigned(FB_BASE) + 16#50C#), x"44444444",
                  s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- 21.2: Write to the register bank immediately after the FB writes
        --       (no pipeline bubble between the two address regions).
        axi_write(CTRL_REG_ADDR, x"00000000",s_axi_awaddr,s_axi_awvalid,s_axi_awready,s_axi_wdata,
                  s_axi_wstrb,s_axi_wvalid,s_axi_wready,s_axi_bvalid,s_axi_bready,axi_clk);

        -- 21.3: Read back all five locations and verify.
        wait for 50 ns;
        axi_read(std_logic_vector(unsigned(FB_BASE) + 16#500#), rdata,
                 s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata = x"11111111", "Back-to-back: FB word 0",
              test_count, pass_count, fail_count);

        axi_read(std_logic_vector(unsigned(FB_BASE) + 16#504#), rdata,
                 s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata = x"22222222", "Back-to-back: FB word 1",
              test_count, pass_count, fail_count);

        axi_read(std_logic_vector(unsigned(FB_BASE) + 16#508#), rdata,
                 s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata = x"33333333", "Back-to-back: FB word 2",
              test_count, pass_count, fail_count);

        axi_read(std_logic_vector(unsigned(FB_BASE) + 16#50C#), rdata,
                 s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata = x"44444444", "Back-to-back: FB word 3",
              test_count, pass_count, fail_count);

        axi_read(CTRL_REG_ADDR, rdata, s_axi_araddr, s_axi_arvalid, s_axi_arready,
                 s_axi_rdata, s_axi_rvalid, s_axi_rready, axi_clk);
        check(rdata(0) = '0', "Back-to-back: CTRL register correct",
              test_count, pass_count, fail_count);

        ----------------------------------------------------------------------
        -- TEST SUITE 22: HSYNC and VSYNC Polarity
        -- H_SYNC_POL and V_SYNC_POL are constants in vga_pkg.
        -- For 640x400 @ 70 Hz the standard polarity is:
        --   HSYNC negative (active low), VSYNC positive (active high).
        -- Verify the idle (non-pulse) state of each sync signal matches
        -- the expected inactive polarity.
        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Suite 22: Sync Signal Polarity ==="));
        writeline(output, l);

        -- 22.1: During active video HSYNC should be inactive (H_SYNC_POL=0
        --       means active-low pulse; idle = '1').
        wait until rising_edge(vga_video_on);
        wait for CLK_PERIOD_PIXEL * 10;  -- well within active region
        check(vga_hsync = '1', "Polarity: HSYNC idle high during active video",
              test_count, pass_count, fail_count);

        -- 22.2: During active video VSYNC should be inactive.
        --       V_SYNC_POL=1 means active-high pulse; idle = '0'.
        check(vga_vsync = '0', "Polarity: VSYNC idle low during active video",
              test_count, pass_count, fail_count);

        -- 22.3: Capture an HSYNC pulse and verify it is active-low
        --       (goes to '0' during the pulse, '1' otherwise).
        wait until falling_edge(vga_hsync);
        check(vga_hsync = '0', "Polarity: HSYNC pulse is low (active-low)",
              test_count, pass_count, fail_count);
        wait until rising_edge(vga_hsync);
        check(vga_hsync = '1', "Polarity: HSYNC returns high after pulse",
              test_count, pass_count, fail_count);

        -- 22.4: Capture a VSYNC pulse and verify it is active-high
        --       (goes to '1' during the pulse, '0' otherwise).
        wait until rising_edge(vga_vsync);
        check(vga_vsync = '1', "Polarity: VSYNC pulse is high (active-high)",
              test_count, pass_count, fail_count);
        wait until falling_edge(vga_vsync);
        check(vga_vsync = '0', "Polarity: VSYNC returns low after pulse",
              test_count, pass_count, fail_count);

        -- Test Summary


        ----------------------------------------------------------------------
        write(l, string'(""));
        writeline(output, l);
        write(l, string'("=== Test Summary ==="));
        writeline(output, l);
        write(l, string'("Total tests: ") & integer'image(test_count));
        writeline(output, l);
        write(l, string'("Passed: ") & integer'image(pass_count));
        writeline(output, l);
        write(l, string'("Failed: ") & integer'image(fail_count));
        writeline(output, l);
        
        if fail_count = 0 then
            write(l, string'("*** ALL TESTS PASSED ***"));
        else
            write(l, string'("*** SOME TESTS FAILED ***"));
        end if;
        writeline(output, l);
        
        wait for 1 us;
        std.env.stop;
    end process;

end Behavioral;
