/* This file provides functions for managing interrupts on RISCV_pyeatt

   When possible, functions are designed to be drop-in replacements
   for the ARM CMSIS NVIC functions used on the Cortex-M series.
   However, there are some differences in behavior.
*/

#include <irq.h>
#include <interrupts.h>

void INTC_Enable_MTIMER_interrupt()
{
  irq_enable(irq_enabled_mask() | IRQ_MTI_MASK);
}

void INTC_Disable_MTIMER_interrupt()
{
  irq_enable(irq_enabled_mask() & ~IRQ_MTI_MASK);
}


// Global interrupt enable. Must be called to enable any interrupts.  
void INTC_Enable_Global()
{
  irq_global_enable();
}

// Globlal interrupt disable.  Does not affect configuration.  Just
// disables all interrupts from the interrupt controller.
void INTC_Disable_Global()
{
  irq_global_disable();
}

// Enable a device specific interrupt 
void INTC_EnableIRQ (IRQn_Type IRQn)
{
  irq_enable(irq_enabled_mask()|IRQ_PLATFORM_MASK(IRQn));
}


// Disable a device specific interrupt 
void INTC_DisableIRQ (IRQn_Type IRQn)
{
  irq_enable(irq_enabled_mask()&~IRQ_PLATFORM_MASK(IRQn));
}

// Get a device specific interrupt enable status.
uint32_t INTC_GetEnableIRQ (IRQn_Type IRQn)
{
  return (irq_enabled_mask() & IRQ_PLATFORM_MASK(IRQn)) ? 1 : 0;
}

// Get the pending device specific interrupt.
uint32_t INTC_GetPendingIRQ (IRQn_Type IRQn)
{
  return (irq_pending_mask() & IRQ_PLATFORM_MASK(IRQn)) ? 1 : 0;
}

// Set a device specific interrupt to pending.
// Not supported: mip is a read-only passthrough on RISCV_pyeatt.
void INTC_SetPendingIRQ (IRQn_Type IRQn)
{
  while(1);
}

// Clear a device specific interrupt from pending.
// On RISCV_pyeatt, mip bits track the level on the source's interrupt
// line, so the pending bit clears automatically once the source device
// (MTIMER TINT, AXI timer TCSR.TINT, UART status, etc.) is cleared.
// Nothing to do here.
void INTC_ClearPendingIRQ (IRQn_Type IRQn)
{
  (void)IRQn;
}

// Get the device specific interrupt active status
uint32_t INTC_GetActive (IRQn_Type IRQn)
{
  while(1);
}

// Set the priority for an interrupt.
// Priorities are hard-wired on the AMD Xilinx AXI interrupt controller.
// void INTC_SetPriority (IRQn_Type IRQn, uint32_t priority);

// Get the priority of an interrupt. 
// Priorities are hard-wired on the AMD Xilinx AXI interrupt controller.
// uint32_t INTC_GetPriority (IRQn_Type IRQn);

// Encode Priority
// Priorities are hard-wired on the AMD Xilinx AXI interrupt controller.
// uint32_t INTC_EncodePriority (uint32_t PriorityGroup,
// uint32_t PreemptPriority, uint32_t SubPriority);

// Decode the interrupt priority
// Priorities are hard-wired on the AMD Xilinx AXI interrupt controller.
// void INTC_DecodePriority (uint32_t Priority, uint32_t PriorityGroup,
// uint32_t *pPreemptPriority, uint32_t *pSubPriority);

// Read Interrupt Vector 
void* INTC_GetVector (IRQn_Type IRQn)
{
  while(1);
}

// Modify Interrupt Vector 
void INTC_SetVector (IRQn_Type IRQn, void* vector)
{
  while(1);
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

  

