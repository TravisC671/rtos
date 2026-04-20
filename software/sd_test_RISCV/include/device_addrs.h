// This file contains the addresses and interrupt numbers
// for every device on the CENG 448 RTOS system (MicroBlaze V platform).

#ifndef DEVICE_ADDRS_H
#define DEVICE_ADDRS_H

#include <stdint.h>

// Simple assertion macro -- hangs in infinite loop on failure
#ifndef ASSERT
#define ASSERT(x) do { if (!(x)) { while(1); } } while(0)
#endif

// IRQ number for the RISC-V Machine Timer
#define MTIME_IRQ         0
// IRQ number for AXI Timer 0
#define TIMER0_IRQ        1
// IRQ number for AXI Timer 1
#define TIMER1_IRQ        2
// IRQ number for the PM device
#define PM_IRQ            3
// IRQ number for GPIO 0 (push buttons)
#define GPIO0_IRQ         4
// IRQ number for GPIO 1 (slide switches and pmod header GPIO)
#define GPIO1_IRQ         5
// IRQ number for UART 0
#define UART0_IRQ         6
// IRQ number for UART 1
#define UART1_IRQ         7
// IRQ number for VGA controller
#define VGA_IRQ           8
// IRQ number for SD controller
#define SD_IRQ            9

// The AXI interrupt controller
#define AXI_INTC_BASE    ((void*)0x41200000)

// The MTIME counter and compare registers are defined in FreeRTOSConfig.h

// There are four hardware timers (two devices with two channels
// each). The timer that provides systicks is separate and part of the
// MTIME device. For more information on the timers, read the AXI
// Timer LogiCORE Product Guide.
// AXI timer 0
#define TIMER0       ((void*)0x41C00000)  // AXI timer with two channels
// AXI timer 1
#define TIMER1       ((void*)0x41C10000)  // AXI timer with two channels

// For more information on the GPIO devices, read the AXI GPIO
// LogiCORE Product Guide.

// The rgb leds and the five pushbuttons are are in GPIO_0, channels 0 and 1
// The lower 6 bits of RGB_LED control the two RGB LEDS.
#define RGB_LEDS      ((uint32_t*)0x40000000)  // gpio_0.channel1.data
// To enable button interrupts,set the interrupt enable register (ier), to
// 0x02 and set bit 31 of the global interrupt enable register (gier) to 1.
// Read the interrupt status register (isr) to clear the interrupt(s).
#define BUTTONS      ((uint32_t*)0x40000008)  // gpio_0.channel2.data
#define BUTTON_gier  ((uint32_t*)0x4000011C)  // gpio_0.gier
#define BUTTON_ier   ((uint32_t*)0x40000128)  // gpio_0.ier
#define BUTTON_isr   ((uint32_t*)0x40000120)  // gpio_0.isr

// The 16 LEDs and the DIP switches are on gpio_1 channels 1 and 2
// The lower 16 bits of LEDS control the 16 LEDS.
#define LEDS         ((uint32_t*)0x40010000)  // gpio_1.channel0.data
// To enable interrupts when a switch changes,,set the interrupt
// enable register (ier), to 0x02 and set bit 31 of the global
// interrupt enable register (gier) to 1.  Read the interrupt status
// register (isr) to clear the interrupt(s).
#define SWITCHES       ((uint32_t*)0x40010008)    // gpio_1.channel1.data
#define SWITCHES_gier  ((uint32_t*)0x4001011C)  // gpio_1.gier
#define SWITCHES_ier   ((uint32_t*)0x40010128)  // gpio_1.ier
#define SWITCHES_isr   ((uint32_t*)0x40010120)  // gpio_1.isr

// GPIO2 provides 8 bits of GPIO, initialized to all inputs, connected
// to PMOD header JA.  Set a bit in the dirs register to 0 to make the
// corresponding data bit be an output.  To enable interrupts on input
// pins, set the interrupt enable register (ier), to 0x02 and set bit
// 31 of the global interrupt enable register (gier) to 1.  Read the
// interrupt status register (isr) to find out which line generated
// the interrupt and to clear the interrupt(s).  For more information,
// read the AXI GPIO LogiCORE Product Guide.
#define GPIO_bits    ((uint32_t*)0x40020000)  // gpio_2.channel1.data
#define GPIO_tris    ((uint32_t*)0x40020000)  // gpio_2.channel1.tris
#define GPIO_gier    ((uint32_t*)0x4002011C)  // gpio_2.gier
#define GPIO_ier     ((uint32_t*)0x40020128)  // gpio_2.ier
#define GPIO_isr     ((uint32_t*)0x40020120)  // gpio_2.isr

// Our system includes the PM device designed in CENG 242 (Digital
// Systems), configured for 5 channels.
// PM channel 0 is connected to the audio jack.
#define PMaudio_ctrl    ((void*)0x44A30000) // PM control register
#define PMaudio_div     ((void*)0x44A30004) // PM clock divider register
#define PMaudio_base    ((void*)0x44A30008) // PM base freq div register
#define PMaudio_duty    ((void*)0x44A3000C) // PM duty cycle register
// The rest of them are routed to PMOD header JB.
// Channel 1 PM output is on JB 1
// Channel 1 PM enabled signal is on JB 7
#define PM1_ctrl    ((void*)0x44A30010) // PM control register
#define PM1_div     ((void*)0x44A30014) // PM clock divider register
#define PM1_base    ((void*)0x44A30018) // PM base frequency div register
#define PM1_duty    ((void*)0x44A3001C) // PM duty cycle register

// Channel 2 PM output is on JB 2
// Channel 2 PM enabled signal is on JB 8
#define PM2_ctrl    ((void*)0x44A30020) // PM control register
#define PM2_div     ((void*)0x44A30024) // PM clock divider register
#define PM2_base    ((void*)0x44A30028) // PM base frequency div register
#define PM2_duty    ((void*)0x44A3002C) // PM duty cycle register

// Channel 3 PM output is on JB 3
// Channel 3 PM enabled signal is on JB 9
#define PM3_ctrl    ((void*)0x44A30030) // PM control register
#define PM3_div     ((void*)0x44A30034) // PM clock divider register
#define PM3_base    ((void*)0x44A30038) // PM base frequency div register
#define PM3_duty    ((void*)0x44A3003C) // PM duty cycle register

// Channel 4 PM output is on JB 10
// Channel 4 PM enabled signal is on JB 10
#define PM4_ctrl    ((void*)0x44A30040) // PM control register
#define PM4_div     ((void*)0x44A30044) // PM clock divider register
#define PM4_base    ((void*)0x44A30048) // PM base frequency div register
#define PM4_duty    ((void*)0x44A3004C) // PM duty cycle register

// 16550D UARTs.  For more information, For more information on the
// UARTs, read the AXI UART LogiCORE Product Guide.

// UART0 is routed to the Digilent USB/UART device which routes the
// signals through the USB cable.
// AXI peripheral base is 0x44A10000; 16550 registers start at +0x1000.
#define UART0_base ((void*)0x44A11000)
#define UART0_RBR  ((volatile uint32_t*)0x44A11000) // Receiver Buffer Register
#define UART0_THR  ((volatile uint32_t*)0x44A11000) // Transmitter Holding Register
#define UART0_IER  ((volatile uint32_t*)0x44A11004) // Interrupt Enable Register
#define UART0_IIR  ((volatile uint32_t*)0x44A11008) // Interrupt Identification Register
#define UART0_FCR  ((volatile uint32_t*)0x44A11008) // FIFO Control Register
#define UART0_LCR  ((volatile uint32_t*)0x44A1100C) // Line Control Register
#define UART0_MCR  ((volatile uint32_t*)0x44A11010) // Modem Control Register
#define UART0_LSR  ((volatile uint32_t*)0x44A11014) // Line Status Register
#define UART0_MSR  ((volatile uint32_t*)0x44A11018) // Modem Status Register
#define UART0_SCR  ((volatile uint32_t*)0x44A1101C) // Scratch Register
#define UART0_DLL  ((volatile uint32_t*)0x44A11000) // Divisor Latch (LSB) Register
#define UART0_DLH  ((volatile uint32_t*)0x44A11004) // Divisor Latch (MSB) Register

// You can use a USB to serial adapter to access UART1.
// UART1 RXD is on PMOD header JC pin 1
// UART1 TXD is on PMOD header JC pin 2
#define UART1_base     ((void*)0x44A21000)

// VGA Controller
#define VGA_BASE       0x48000000

// SD Host Controller (SD Simplified Specification compatible)
// For register access, use:
//   REG32(addr) for 32-bit registers
//   REG16(addr) for 16-bit registers
//   REG8(addr)  for 8-bit registers
#define REG32(a) (*(volatile uint32_t*)(a))
#define REG16(a) (*(volatile uint16_t*)(a))
#define REG8(a)  (*(volatile uint8_t*)(a))

#define SD_BASE              0x44A40000
#define SD_SDMA_ADDR         (SD_BASE + 0x00) // SDMA System Address (32-bit)
#define SD_BLOCK_SIZE        (SD_BASE + 0x04) // Block Size (16-bit)
#define SD_BLOCK_COUNT       (SD_BASE + 0x06) // Block Count (16-bit)
#define SD_ARGUMENT          (SD_BASE + 0x08) // Command Argument (32-bit)
#define SD_TRANSFER_MODE     (SD_BASE + 0x0C) // Transfer Mode (16-bit)
#define SD_COMMAND           (SD_BASE + 0x0E) // Command Register (16-bit)
#define SD_RESPONSE0         (SD_BASE + 0x10) // Response[31:0]
#define SD_RESPONSE1         (SD_BASE + 0x14) // Response[63:32]
#define SD_RESPONSE2         (SD_BASE + 0x18) // Response[95:64]
#define SD_RESPONSE3         (SD_BASE + 0x1C) // Response[127:96]
#define SD_BUFFER_PORT       (SD_BASE + 0x20) // Buffer Data Port (32-bit)
#define SD_PRESENT_STATE     (SD_BASE + 0x24) // Present State (32-bit)
#define SD_HOST_CTRL1        (SD_BASE + 0x28) // Host Control 1 (8-bit)
#define SD_POWER_CTRL        (SD_BASE + 0x29) // Power Control (8-bit)
#define SD_BLOCK_GAP_CTRL    (SD_BASE + 0x2A) // Block Gap Control (8-bit)
#define SD_WAKEUP_CTRL       (SD_BASE + 0x2B) // Wakeup Control (8-bit)
#define SD_CLOCK_CTRL        (SD_BASE + 0x2C) // Clock Control (16-bit)
#define SD_TIMEOUT_CTRL      (SD_BASE + 0x2E) // Timeout Control (8-bit)
#define SD_SW_RESET          (SD_BASE + 0x2F) // Software Reset (8-bit)
#define SD_NORM_INT_STATUS   (SD_BASE + 0x30) // Normal Interrupt Status (16-bit)
#define SD_ERR_INT_STATUS    (SD_BASE + 0x32) // Error Interrupt Status (16-bit)
#define SD_NORM_INT_STAT_EN  (SD_BASE + 0x34) // Normal Int Status Enable (16-bit)
#define SD_ERR_INT_STAT_EN   (SD_BASE + 0x36) // Error Int Status Enable (16-bit)
#define SD_NORM_INT_SIG_EN   (SD_BASE + 0x38) // Normal Int Signal Enable (16-bit)
#define SD_ERR_INT_SIG_EN    (SD_BASE + 0x3A) // Error Int Signal Enable (16-bit)
#define SD_AUTO_CMD_ERR      (SD_BASE + 0x3C) // Auto CMD Error Status (16-bit, RO)
#define SD_HOST_CONTROL_2    (SD_BASE + 0x3E) // Host Control 2 (16-bit)
#define SD_CAPABILITIES_LOW  (SD_BASE + 0x40) // Capabilities [31:0]
#define SD_CAPABILITIES_HIGH (SD_BASE + 0x44) // Capabilities [63:32]
#define SD_CARD_INFO         (SD_BASE + 0xE0) // Card Info: RCA, Type, Status
#define SD_VERSION           (SD_BASE + 0xFC) // Slot Int Status / Controller Version

// SD Present State Register bit masks
#define SD_STATE_CMD_INHIBIT      (1 << 0)  // Command Inhibit (CMD)
#define SD_STATE_CMD_INHIBIT_DAT  (1 << 1)  // Command Inhibit (DAT)
#define SD_STATE_DAT_INHIBIT      (1 << 1)  // Command Inhibit (DAT)
#define SD_STATE_DAT_ACTIVE       (1 << 2)  // DAT Line Active
#define SD_STATE_WRITE_ACTIVE     (1 << 8)  // Write Transfer Active
#define SD_STATE_READ_ACTIVE      (1 << 9)  // Read Transfer Active
#define SD_STATE_BUF_WRITE_EN     (1 << 10) // Buffer Write Enable
#define SD_STATE_BUF_READ_EN      (1 << 11) // Buffer Read Enable
#define SD_STATE_CARD_INSERTED    (1 << 16) // Card Inserted
#define SD_STATE_CARD_STABLE      (1 << 17) // Card State Stable
#define SD_STATE_CARD_DETECT_PIN  (1 << 18) // Card Detect Pin Level
#define SD_STATE_WRITE_PROTECT    (1 << 19) // Write Protect Switch Level
#define SD_STATE_DAT_LEVEL        (0xF << 20) // DAT[3:0] Line Level
#define SD_STATE_CMD_LEVEL        (1 << 24) // CMD Line Level

// SD Normal Interrupt Status bits
#define SD_INT_CMD_COMPLETE       (1 << 0)  // Command Complete
#define SD_INT_XFER_COMPLETE      (1 << 1)  // Transfer Complete
#define SD_INT_DMA_INT            (1 << 3)  // DMA Interrupt
#define SD_INT_BUF_WRITE_READY    (1 << 4)  // Buffer Write Ready
#define SD_INT_BUF_READ_READY     (1 << 5)  // Buffer Read Ready
#define SD_INT_CARD_INSERT        (1 << 6)  // Card Insertion
#define SD_INT_CARD_REMOVE        (1 << 7)  // Card Removal
#define SD_INT_ERROR              (1 << 15) // Error Interrupt

// SD Transfer Mode register bit masks
#define SD_TM_DMA_ENABLE          (1 << 0)  // DMA Enable
#define SD_TM_BLOCK_COUNT_EN      (1 << 1)  // Block Count Enable
#define SD_TM_AUTO_CMD12_EN       (1 << 2)  // Auto CMD12 Enable
#define SD_TM_DATA_DIR_READ       (1 << 4)  // Data Transfer Direction: 1=read
#define SD_TM_MULTI_BLOCK         (1 << 5)  // Multi Block Select

// SD Error Interrupt Status bits
#define SD_ERR_CMD_TIMEOUT        (1 << 0)  // Command Timeout Error
#define SD_ERR_CMD_CRC            (1 << 1)  // Command CRC Error
#define SD_ERR_CMD_END_BIT        (1 << 2)  // Command End Bit Error
#define SD_ERR_CMD_INDEX          (1 << 3)  // Command Index Error
#define SD_ERR_DATA_TIMEOUT       (1 << 4)  // Data Timeout Error
#define SD_ERR_DATA_CRC           (1 << 5)  // Data CRC Error
#define SD_ERR_DATA_END_BIT       (1 << 6)  // Data End Bit Error
#define SD_ERR_AUTO_CMD12         (1 << 8)  // Auto CMD12 Error

// SD Software Reset bits
#define SD_RESET_ALL              0x01
#define SD_RESET_CMD              0x02
#define SD_RESET_DAT              0x04

// SD Clock Control bits
#define SD_CLK_INT_EN             (1 << 0)  // Internal Clock Enable
#define SD_CLK_INT_STABLE         (1 << 1)  // Internal Clock Stable (read-only)
#define SD_CLK_SD_EN              (1 << 2)  // SD Clock Enable

// SD Card Info register fields (register 0xE0)
// NOTE: With software initialization, this register reads as zero.
// These defines are retained for reference only.
#define SD_CARD_INFO_RCA_MASK     0x0000FFFF // RCA (bits 15:0) — unused
#define SD_CARD_INFO_RCA_SHIFT    0
#define SD_CARD_INFO_TYPE_MASK    0x00030000 // Card type (bits 17:16) — unused
#define SD_CARD_INFO_TYPE_SHIFT   16
#define SD_CARD_INFO_INIT_BUSY    (1 << 18)  // unused (no HW init controller)
#define SD_CARD_INFO_INIT_DONE    (1 << 19)  // unused (no HW init controller)
#define SD_CARD_INFO_INIT_ERROR   (1 << 20)  // unused (no HW init controller)

#endif // DEVICE_ADDRS_H
