
/* This file provides functions for the AMD Xilinx AXI interrupt
   controller.  It does not support cascaded interrupt controllers, so
   is limited to handling 32 interrupts.

   When possible, functions are designed to be drop-in replacements
   for the ARM CMSIS NVIC functions used on the Cortex-M series.
   However, there are some differences in behavior.
   
   From the documentation:
   During power-up or reset, the AXI INTC core is initialized to a
   state where all interrupt inputs and the interrupt request output
   are disabled. In order for the AXI INTC core to accept interrupts
   and request service, the following steps are required:

   1. Each bit in the IER corresponding to an interrupt must be set to
      1. This allows the AXI INTC core to begin accepting interrupt
      input signals and software interrupts. INT0 has the highest
      priority, and it corresponds to the least significant bit (LSB)
      in the IER.

   2. The MER must be programmed based on the intended use of the AXI
      INTC core. There are two bits in the MER: the Hardware Interrupt
      Enable (HIE) and the Master IRQ Enable (ME). The ME bit must be
      set to enable the interrupt request output.

   3. If software testing of hardware interrupts is to be performed,
      the HIE bit must remain at its reset value of 0. Software
      testing can now proceed by writing a 1 to any bit position in
      the ISR that corresponds to an existing interrupt input or
      software interrupt. A corresponding interrupt request is
      generated if that interrupt is enabled, and interrupt handling
      proceeds normally.

   4. After software testing of hardware interrupts has been
      completed, or if testing is not performed, a 1 must be written
      to the HIE bit, which enables the hardware interrupt inputs and
      disables any further software generated hardware interrupts.

   5. After 1 is written to the HIE bit, any further writes to this bit
      have no effect.
*/

#include <device_addrs.h>
#include <interrupts.h>

// Address of the interrupt controller

#ifndef ASSERT
#define ASSERT(x)
#endif

typedef struct {
  volatile uint32_t ISR; // Interrupt Status Register
  volatile uint32_t IPR; // Interrupt Pending Register
  volatile uint32_t IER; // Interrupt Enable Register
  volatile uint32_t IAR; // Interrupt Acknowledge Register
  volatile uint32_t SIE; // Set Interrupt Enables
  volatile uint32_t CIE; // Clear Interrupt Enables
  volatile uint32_t IVR; // Interrupt Vector Register
  volatile uint32_t MER; // Master Enable Register
  volatile uint32_t IMR; // Interrupt Mode Register
  volatile uint32_t ILR; // Interrupt Level Register
  uint32_t unused1[54];
  volatile uint32_t IVAR[32];  // Interrupt Vector Address Registers
  uint32_t unused2[32];
  volatile uint64_t IVEAR[32]; // Interrupt Vector Extended Address Registers
} AXI_INTC_t;

AXI_INTC_t *intc = (AXI_INTC_t*)AXI_INTC_BASE;

// Reset the INTC to a known state.
void INTC_Init()
{
  int i;
  // Disable master enable
  intc->MER = 0;
  // Disable all individual interrupts
  intc->CIE = 0xFFFFFFFF;
  // Acknowledge all pending interrupts
  intc->IAR = 0xFFFFFFFF;
  // Zero all IVAR entries so stale vectors can't cause wild jumps
  for (i = 0; i < 32; i++)
    intc->IVAR[i] = 0;
}

// Global interrupt enable. Must be called to enable any interrupts.
void INTC_Enable_Global()
{
  intc->MER = 0b11;
}

// Globlal interrupt disable.  Does not affect configuration.  Just
// disables all interrupts from the interrupt controller.
void INTC_Disable_Global()
{
  intc->MER = 0;
}

// Enable a device specific interrupt
void INTC_EnableIRQ (IRQn_Type IRQn)
{
  ASSERT(IRQn < 32);
  intc->SIE = 1 << IRQn;
}

// Enable a device specific interrupt 
void INTC_DisableIRQ (IRQn_Type IRQn)
{
  ASSERT(IRQn < 32);
  intc->CIE = 1 << IRQn;
}

// Get a device specific interrupt enable status.
uint32_t INTC_GetEnableIRQ (IRQn_Type IRQn)
{
  ASSERT(IRQn < 32);
  return (intc->IER >> IRQn) & 1;
}

// Get the pending device specific interrupt.
uint32_t INTC_GetPendingIRQ (IRQn_Type IRQn)
{
  ASSERT(IRQn < 32);
  return (intc->IPR >> IRQn) & 1;
}

// Set a device specific interrupt to pending.
void INTC_SetPendingIRQ (IRQn_Type IRQn)
{
  ASSERT(IRQn < 32);
  intc->MER = 0;
  intc->ISR |= (1 << IRQn);
  intc->MER = 0b11;
}

// Clear a device specific interrupt from pending
void INTC_ClearPendingIRQ (IRQn_Type IRQn)
{
  ASSERT(IRQn < 32);
  intc->IAR = (1 << IRQn);
}

// Get the device specific interrupt active status
uint32_t INTC_GetActive (IRQn_Type IRQn)
{
  ASSERT(IRQn < 32);
  return (intc->ISR >> IRQn) & 1;
}

// Set the priority for an interrupt.
// Priorities are hard-wired on the AMD Xilinx AXI interrupt controller.
// void INTC_SetPriority (IRQn_Type IRQn, uint32_t priority);

// Get the priority of an interrupt. 
// Priorities are hard-wired on the AMD Xilinx AXI interrupt controller.
// uint32_t INTC_GetPriority (IRQn_Type IRQn);

// Encode Priority
// Priorities are hard-wired on the AMD Xilinx AXI interrupt controller.
// uint32_t INTC_EncodePriority (uint32_t PriorityGroup, uint32_t PreemptPriority, uint32_t SubPriority);

// Decode the interrupt priority
// Priorities are hard-wired on the AMD Xilinx AXI interrupt controller.
// void INTC_DecodePriority (uint32_t Priority, uint32_t PriorityGroup, uint32_t *pPreemptPriority, uint32_t *pSubPriority);

// Read Interrupt Vector 
void* INTC_GetVector (IRQn_Type IRQn)
{
  ASSERT(IRQn < 32);
  return (void *)intc->IVAR[IRQn];
}

// Modify Interrupt Vector 
void INTC_SetVector (IRQn_Type IRQn, void* vector)
{
  uint32_t enable;
  ASSERT(IRQn < 32);
  // Get current enable/disable for IRQn
  enable = intc->IER & (1 << IRQn);
  // Disable IRQn
  intc->CIE = 1 << IRQn;
  // Set the vector
  intc->IVAR[IRQn] = (uint32_t)vector;
  // Restore original enable/disable state
  intc->SIE = enable;
}

// Application interrupt handler for FreeRTOS RISC-V port.
// Overrides the weak default in portASM.S that just spins.
// Called when mcause = 0x8000000b (machine external interrupt).
// Reads the AXI INTC to determine which device interrupted,
// dispatches to the registered handler, and acknowledges.
void freertos_risc_v_application_interrupt_handler(void)
{
  uint32_t irqn;
  void (*handler)(void);

  // Service one interrupt per call.  If other interrupts are still
  // pending, the INTC output stays asserted and the CPU will
  // re-enter the trap immediately after mret.
  irqn = intc->IVR;
  if (irqn < 32) {
    handler = (void (*)(void))intc->IVAR[irqn];
    if (handler) {
      handler();
    }
    intc->IAR = 1 << irqn;
  }
}

// Reset the system.
// Not supported
// void INTC_SystemReset (void);

// Get Interrupt Target State.
// Not supported 
// uint32_t INTC_GetTargetState (IRQn_Type IRQn);

// Set Interrupt Target State.
// Not supported 
// uint32_t INTC_SetTargetState (IRQn_Type IRQn);

// Clear Interrupt Target State.
// Not supported 
// uint32_t INTC_ClearTargetState (IRQn_Type IRQn);

  

