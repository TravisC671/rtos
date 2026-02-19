// This file contains the addresses and interrupt numbers
// for every device on the CENG 448 RTOS system.

// IRQ number for AXI Timer 0
#define TIMER0_IRQ        0
// IRQ number for AXI Timer 1
#define TIMER1_IRQ        1
// IRQ number for GPIO 0 (push buttons)
#define GPIO0_IRQ         2
// IRQ number for GPIO 1 (slide switches and pmod header GPIO)
#define GPIO1_IRQ         3
// IRQ number for the PM device
#define PM_IRQ            4
// IRQ number for UART 0 
#define UART0_IRQ         5 
// IRQ number for UART 1
#define UART1_IRQ         6

// There are four hardware timers (two devices with two channels
// each). The timer that provides systicks is separate and part of the
// Cortex-M3 core. For more information on the timers, read the AXI
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
#define PMaudio_ctrl    ((void*)0x44A00000) // PM control register
#define PMaudio_div     ((void*)0x44A00004) // PM clock divider register
#define PMaudio_base    ((void*)0x44A00008) // PM base freq div register
#define PMaudio_duty    ((void*)0x44A0000C) // PM duty cycle register
// The rest of them are routed to PMOD header JB.
// Channel 1 PM output is on JB 1
// Channel 1 PM enabled signal is on JB 7
#define PM1_ctrl    ((void*)0x44A00010) // PM control register
#define PM1_div     ((void*)0x44A00014) // PM clock divider register
#define PM1_base    ((void*)0x44A00018) // PM base frequency div register
#define PM1_duty    ((void*)0x44A0001C) // PM duty cycle register

// Channel 2 PM output is on JB 2
// Channel 2 PM enabled signal is on JB 8
#define PM2_ctrl    ((void*)0x44A00020) // PM control register
#define PM2_div     ((void*)0x44A00024) // PM clock divider register
#define PM2_base    ((void*)0x44A00028) // PM base frequency div register
#define PM2_duty    ((void*)0x44A0002C) // PM duty cycle register

// Channel 3 PM output is on JB 3
// Channel 3 PM enabled signal is on JB 9
#define PM3_ctrl    ((void*)0x44A00030) // PM control register
#define PM3_div     ((void*)0x44A00034) // PM clock divider register
#define PM3_base    ((void*)0x44A00038) // PM base frequency div register
#define PM3_duty    ((void*)0x44A0003C) // PM duty cycle register

// Channel 4 PM output is on JB 10
// Channel 4 PM enabled signal is on JB 10
#define PM4_ctrl    ((void*)0x44A00040) // PM control register
#define PM4_div     ((void*)0x44A00044) // PM clock divider register
#define PM4_base    ((void*)0x44A00048) // PM base frequency div register
#define PM4_duty    ((void*)0x44A0004C) // PM duty cycle register

// 16550D UARTs.  For more information, For more information on the
// UARTs, read the AXI UART LogiCORE Product Guide.

// UART0 is routed to the Digilent USB/UART device which routes the
// signals through the USB cable.
#define UART0_base ((void*)0x44A10000) 
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
#define UART1_base     ((void*)0x44A20000) 

