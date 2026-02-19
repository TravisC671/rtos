// This file contains the addresses for all of the registers in every
// device on our system.


// For more information on the GPIO devices, read the AXI GPIO
// LogiCORE Product Guide.

// The rgb led and the five pushbuttons are are in GPIO_0, channels 0 and 1
#define rgb_led      ((uint32_t*)0x40000000)  // gpio_0.channel1.data
// To enable button interrupts,set the interrupt enable register (ier), to 
// 0x02 and set bit 31 of the global interrupt enable register (gier) to 1.
// Read the interrupt status register (isr) to clear the interrupt(s).
#define buttons      ((uint32_t*)0x40000008)  // gpio_0.channel2.data
#define button_gier  ((uint32_t*)0x4000011C)  // gpio_0.gier
#define button_ier   ((uint32_t*)0x40000128)  // gpio_0.ier
#define button_isr   ((uint32_t*)0x40000120)  // gpio_0.isr

// The switches and other PMOD gpio bits are on gpio_1 channels 1 and 2
#define switches     ((uint32_t*)0x40010000)  // gpio_1.channel1.data
// 20 bits of GPIO, initialized to all inputs, connected to the PMOD
// headers.  The bottom eight bits of the GPIO device are routed to
// JA1 through JA8.  the next eight bits go to JB, and the top eight
// bits go to JC. Set a bit in the dirs register to 0 to make the
// corresponding data bit be an output.  To enable interrupts on input
// pins, set the interrupt enable register (ier), to 0x02 and set bit
// 31 of the global interrupt enable register (gier) to 1.  Read the
// interrupt status register (isr) to find out which line generated
// the interrupt and to clear the interrupt(s).  For more information,
// read the AXI GPIO LogiCORE Product Guide.
#define GPIO_bits    ((uint32_t*)0x40010000)  // gpio_1.channel2.data
#define GPIO_tris    ((uint32_t*)0x40010000)  // gpio_1.channel2.tris
#define GPIO_gier    ((uint32_t*)0x4001011C)  // gpio_1.gier
#define GPIO_ier     ((uint32_t*)0x40010128)  // gpio_1.ier
#define GPIO_isr     ((uint32_t*)0x40010120)  // gpio_1.isr

// 16 discrete LEDs are on gpio_2 channel 1. It is not software configurable.
#define LED          ((uint32_t*)0x40020000)  // gpio_2.channel1.data

// There are four hardware timers (two devices with two channels
// each). The timer that provides systicks is separate and part of the
// Cortex-M3 core. For more information on the timers, read the AXI
// Timer LogiCORE Product Guide.
// AXI timer 0
#define TIMER0       ((uint32_t*)0x41C00000)  // AXI timer with two channels
// AXI timer 1
#define TIMER1       ((uint32_t*)0x41C10000)  // AXI timer with two channels

// Our system includes the PWM device designed in CENG 342 (Digital
// Systems), configured for 5 channels.
// PWM channel 0 is connected to the audio jack.
#define PWMaudio_ctrl    ((uint32_t*)0x44A00000) // PWM control register
#define PWMaudio_div     ((uint32_t*)0x44A00004) // PWM clock divider register
#define PWMaudio_base    ((uint32_t*)0x44A00008) // PWM base freq div register
#define PWMaudio_duty    ((uint32_t*)0x44A0000C) // PWM duty cycle register
// The rest of them are routed to PMOD header C.
#define PWM1_ctrl    ((uint32_t*)0x44A00010) // PWM control register
#define PWM1_div     ((uint32_t*)0x44A00014) // PWM clock divider register
#define PWM1_base    ((uint32_t*)0x44A00018) // PWM base frequency div register
#define PWM1_duty    ((uint32_t*)0x44A0001C) // PWM duty cycle register

#define PWM2_ctrl    ((uint32_t*)0x44A00020) // PWM control register
#define PWM2_div     ((uint32_t*)0x44A00024) // PWM clock divider register
#define PWM2_base    ((uint32_t*)0x44A00028) // PWM base frequency div register
#define PWM2_duty    ((uint32_t*)0x44A0002C) // PWM duty cycle register

#define PWM3_ctrl    ((uint32_t*)0x44A00030) // PWM control register
#define PWM3_div     ((uint32_t*)0x44A00034) // PWM clock divider register
#define PWM3_base    ((uint32_t*)0x44A00038) // PWM base frequency div register
#define PWM3_duty    ((uint32_t*)0x44A0003C) // PWM duty cycle register

#define PWM4_ctrl    ((uint32_t*)0x44A00040) // PWM control register
#define PWM4_div     ((uint32_t*)0x44A00044) // PWM clock divider register
#define PWM4_base    ((uint32_t*)0x44A00048) // PWM base frequency div register
#define PWM4_duty    ((uint32_t*)0x44A0004C) // PWM duty cycle register

// 16550D UART.  For more information, For more information on the
// UART(s), read the AXI UART LogiCORE Product Guide.
#define UART0_RBR     ((uint32_t*)0x44A10000) // Receiver Buffer Register
#define UART0_THR     ((uint32_t*)0x44A10000) // Transmitter Holding Register
#define UART0_IER     ((uint32_t*)0x44A10004) // Interrupt Enable Register
#define UART0_IIR     ((uint32_t*)0x44A10008) // Interrupt Identification Reg.
#define UART0_FCR     ((uint32_t*)0x44A10008) // FIFO Control Register
#define UART0_LCR     ((uint32_t*)0x44A1000C) // Line Control Register
#define UART0_MCR     ((uint32_t*)0x44A10010) // Modem Control Register
#define UART0_LSR     ((uint32_t*)0x44A10014) // Line Status Register
#define UART0_MSR     ((uint32_t*)0x44A10018) // Modem Status Register
#define UART0_SCR     ((uint32_t*)0x44A1001C) // Scratch Register
#define UART0_DLL     ((uint32_t*)0x44A10000) // Divisor Latch (LSB) Register
#define UART0_DLH     ((uint32_t*)0x44A10004) // Divisor Latch (MSB) Register

// LDP-020 UART.  We also have a partially functional LDP-020 UART
// The one we designed in CENG 342 (Digital Systems).
#define UART2         ((uint32_t*)0x44A20000) // First register
