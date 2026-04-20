// This file contains the addresses and interrupt numbers
// for every device on the CENG 448 RTOS system (RISC-V / MicroBlaze V).

#ifndef DEVICE_ADDRS_H
#define DEVICE_ADDRS_H

#include <stdint.h>

// Simple assertion macro -- hangs in infinite loop on failure
#ifndef ASSERT
#define ASSERT(x) do { if (!(x)) { while(1); } } while(0)
#endif

// -----------------------------------------------------------------------
// AXI Interrupt Controller
// -----------------------------------------------------------------------
#define AXI_INTC_BASE        ((void*)0x41200000)

// -----------------------------------------------------------------------
// IRQ numbers (AXI INTC input assignments for MicroBlaze V platform)
// -----------------------------------------------------------------------
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
// IRQ number for the VGA Controller
#define VGA_IRQ           8
// IRQ number for the SD Card Controller
#define SD_IRQ            9

// -----------------------------------------------------------------------
// MTIME (machine timer)
// -----------------------------------------------------------------------
#define MTIME_TIMER       ((void*)0x44A00000)

// -----------------------------------------------------------------------
// AXI Timers
// -----------------------------------------------------------------------
#define TIMER0       ((void*)0x41C00000)
#define TIMER1       ((void*)0x41C10000)

// -----------------------------------------------------------------------
// GPIO
// -----------------------------------------------------------------------
#define RGB_LEDS      ((uint32_t*)0x40000000)
#define BUTTONS      ((uint32_t*)0x40000008)
#define BUTTON_gier  ((uint32_t*)0x4000011C)
#define BUTTON_ier   ((uint32_t*)0x40000128)
#define BUTTON_isr   ((uint32_t*)0x40000120)

#define LEDS         ((uint32_t*)0x40010000)
#define SWITCHES       ((uint32_t*)0x40010008)
#define SWITCHES_gier  ((uint32_t*)0x4001011C)
#define SWITCHES_ier   ((uint32_t*)0x40010128)
#define SWITCHES_isr   ((uint32_t*)0x40010120)

#define GPIO_bits    ((uint32_t*)0x40020000)
#define GPIO_tris    ((uint32_t*)0x40020000)
#define GPIO_gier    ((uint32_t*)0x4002011C)
#define GPIO_ier     ((uint32_t*)0x40020128)
#define GPIO_isr     ((uint32_t*)0x40020120)

// -----------------------------------------------------------------------
// PM device (5 channels, APB bridge at 0x44A30000)
// -----------------------------------------------------------------------
#define PMaudio_ctrl    ((void*)0x44A30000)
#define PMaudio_div     ((void*)0x44A30004)
#define PMaudio_base    ((void*)0x44A30008)
#define PMaudio_duty    ((void*)0x44A3000C)
#define PM1_ctrl    ((void*)0x44A30010)
#define PM1_div     ((void*)0x44A30014)
#define PM1_base    ((void*)0x44A30018)
#define PM1_duty    ((void*)0x44A3001C)
#define PM2_ctrl    ((void*)0x44A30020)
#define PM2_div     ((void*)0x44A30024)
#define PM2_base    ((void*)0x44A30028)
#define PM2_duty    ((void*)0x44A3002C)
#define PM3_ctrl    ((void*)0x44A30030)
#define PM3_div     ((void*)0x44A30034)
#define PM3_base    ((void*)0x44A30038)
#define PM3_duty    ((void*)0x44A3003C)
#define PM4_ctrl    ((void*)0x44A30040)
#define PM4_div     ((void*)0x44A30044)
#define PM4_base    ((void*)0x44A30048)
#define PM4_duty    ((void*)0x44A3004C)

// -----------------------------------------------------------------------
// 16550D UARTs
// -----------------------------------------------------------------------
#define UART0_base ((void*)0x44A10000)
#define UART0_RBR  ((volatile uint32_t*)0x44A11000)
#define UART0_THR  ((volatile uint32_t*)0x44A11000)
#define UART0_IER  ((volatile uint32_t*)0x44A11004)
#define UART0_IIR  ((volatile uint32_t*)0x44A11008)
#define UART0_FCR  ((volatile uint32_t*)0x44A11008)
#define UART0_LCR  ((volatile uint32_t*)0x44A1100C)
#define UART0_MCR  ((volatile uint32_t*)0x44A11010)
#define UART0_LSR  ((volatile uint32_t*)0x44A11014)
#define UART0_MSR  ((volatile uint32_t*)0x44A11018)
#define UART0_SCR  ((volatile uint32_t*)0x44A1101C)
#define UART0_DLL  ((volatile uint32_t*)0x44A11000)
#define UART0_DLH  ((volatile uint32_t*)0x44A11004)

#define UART1_base     ((void*)0x44A20000)

// -----------------------------------------------------------------------
// VGA Controller
// -----------------------------------------------------------------------
#define VGA_BASE         ((void*)0x48000000)

// -----------------------------------------------------------------------
// SD Host Controller (SD Simplified Specification compatible)
// -----------------------------------------------------------------------
#define REG32(a) (*(volatile uint32_t*)(a))
#define REG16(a) (*(volatile uint16_t*)(a))
#define REG8(a)  (*(volatile uint8_t*)(a))

#define SD_BASE              0x44A40000
#define SD_SDMA_ADDR         (SD_BASE + 0x00)
#define SD_BLOCK_SIZE        (SD_BASE + 0x04)
#define SD_BLOCK_COUNT       (SD_BASE + 0x06)
#define SD_ARGUMENT          (SD_BASE + 0x08)
#define SD_TRANSFER_MODE     (SD_BASE + 0x0C)
#define SD_COMMAND           (SD_BASE + 0x0E)
#define SD_RESPONSE0         (SD_BASE + 0x10)
#define SD_RESPONSE1         (SD_BASE + 0x14)
#define SD_RESPONSE2         (SD_BASE + 0x18)
#define SD_RESPONSE3         (SD_BASE + 0x1C)
#define SD_BUFFER_PORT       (SD_BASE + 0x20)
#define SD_PRESENT_STATE     (SD_BASE + 0x24)
#define SD_HOST_CTRL1        (SD_BASE + 0x28)
#define SD_POWER_CTRL        (SD_BASE + 0x29)
#define SD_BLOCK_GAP_CTRL    (SD_BASE + 0x2A)
#define SD_WAKEUP_CTRL       (SD_BASE + 0x2B)
#define SD_CLOCK_CTRL        (SD_BASE + 0x2C)
#define SD_TIMEOUT_CTRL      (SD_BASE + 0x2E)
#define SD_SW_RESET          (SD_BASE + 0x2F)
#define SD_NORM_INT_STATUS   (SD_BASE + 0x30)
#define SD_ERR_INT_STATUS    (SD_BASE + 0x32)
#define SD_NORM_INT_STAT_EN  (SD_BASE + 0x34)
#define SD_ERR_INT_STAT_EN   (SD_BASE + 0x36)
#define SD_NORM_INT_SIG_EN   (SD_BASE + 0x38)
#define SD_ERR_INT_SIG_EN    (SD_BASE + 0x3A)
#define SD_AUTO_CMD_ERR      (SD_BASE + 0x3C)
#define SD_HOST_CONTROL_2    (SD_BASE + 0x3E)
#define SD_CAPABILITIES_LOW  (SD_BASE + 0x40)
#define SD_CAPABILITIES_HIGH (SD_BASE + 0x44)
#define SD_CARD_INFO         (SD_BASE + 0xE0)
#define SD_VERSION           (SD_BASE + 0xFC)

// SD Present State Register bit masks
#define SD_STATE_CMD_INHIBIT      (1 << 0)
#define SD_STATE_CMD_INHIBIT_DAT  (1 << 1)
#define SD_STATE_DAT_INHIBIT      (1 << 1)
#define SD_STATE_DAT_ACTIVE       (1 << 2)
#define SD_STATE_WRITE_ACTIVE     (1 << 8)
#define SD_STATE_READ_ACTIVE      (1 << 9)
#define SD_STATE_BUF_WRITE_EN     (1 << 10)
#define SD_STATE_BUF_READ_EN      (1 << 11)
#define SD_STATE_CARD_INSERTED    (1 << 16)
#define SD_STATE_CARD_STABLE      (1 << 17)
#define SD_STATE_CARD_DETECT_PIN  (1 << 18)
#define SD_STATE_WRITE_PROTECT    (1 << 19)
#define SD_STATE_DAT_LEVEL        (0xF << 20)
#define SD_STATE_CMD_LEVEL        (1 << 24)

// SD Normal Interrupt Status bits
#define SD_INT_CMD_COMPLETE       (1 << 0)
#define SD_INT_XFER_COMPLETE      (1 << 1)
#define SD_INT_DMA_INT            (1 << 3)
#define SD_INT_BUF_WRITE_READY    (1 << 4)
#define SD_INT_BUF_READ_READY     (1 << 5)
#define SD_INT_CARD_INSERT        (1 << 6)
#define SD_INT_CARD_REMOVE        (1 << 7)
#define SD_INT_ERROR              (1 << 15)

// SD Transfer Mode register bit masks
#define SD_TM_DMA_ENABLE          (1 << 0)
#define SD_TM_BLOCK_COUNT_EN      (1 << 1)
#define SD_TM_AUTO_CMD12_EN       (1 << 2)
#define SD_TM_DATA_DIR_READ       (1 << 4)
#define SD_TM_MULTI_BLOCK         (1 << 5)

// SD Error Interrupt Status bits
#define SD_ERR_CMD_TIMEOUT        (1 << 0)
#define SD_ERR_CMD_CRC            (1 << 1)
#define SD_ERR_CMD_END_BIT        (1 << 2)
#define SD_ERR_CMD_INDEX          (1 << 3)
#define SD_ERR_DATA_TIMEOUT       (1 << 4)
#define SD_ERR_DATA_CRC           (1 << 5)
#define SD_ERR_DATA_END_BIT       (1 << 6)
#define SD_ERR_AUTO_CMD12         (1 << 8)

// SD Software Reset bits
#define SD_RESET_ALL              0x01
#define SD_RESET_CMD              0x02
#define SD_RESET_DAT              0x04

// SD Clock Control bits
#define SD_CLK_INT_EN             (1 << 0)
#define SD_CLK_INT_STABLE         (1 << 1)
#define SD_CLK_SD_EN              (1 << 2)

// SD Card Info register fields (register 0xE0)
#define SD_CARD_INFO_RCA_MASK     0x0000FFFF
#define SD_CARD_INFO_RCA_SHIFT    0
#define SD_CARD_INFO_TYPE_MASK    0x00030000
#define SD_CARD_INFO_TYPE_SHIFT   16
#define SD_CARD_INFO_INIT_BUSY    (1 << 18)
#define SD_CARD_INFO_INIT_DONE    (1 << 19)
#define SD_CARD_INFO_INIT_ERROR   (1 << 20)

#endif // DEVICE_ADDRS_H
