#ifndef INTERRUPTS_H
#define INTERRUPTS_H

/* This file provides functions for the AMD Xilinx AXI interrupt
   controller.  It does not support cascaded interrupt controllers, so
   is limited to handling 32 interrupts.

   When possible, functions are designed to be drop-in replacements
   for the ARM CMSIS NVIC functions used on the Cortex-M series.
   However, there are some differences in behavior.
*/


#include <stdint.h>

typedef uint32_t IRQn_Type;

// Reset the INTC to a known state.  Disables master enable, clears
// all individual enables, acknowledges all pending interrupts, and
// zeroes all IVAR entries.  Must be called before setting up vectors
// to avoid stale state from a previous run (the INTC is not reset
// when loading via the debugger).
void INTC_Init();

// Global interrupt enable. Must be called to enable any interrupts.
void INTC_Enable_Global();

// Globlal interrupt disable.  Does not affect configuration.  Just
// disables all interrupts from the interrupt controller.
void INTC_Disable_Global();

// Enable a device specific interrupt 
void INTC_EnableIRQ (IRQn_Type IRQn);

// Disable a device specific interrupt 
void INTC_DisableIRQ (IRQn_Type IRQn);

// Get a device specific interrupt enable status.
uint32_t INTC_GetEnableIRQ (IRQn_Type IRQn);

// Get the pending device specific interrupt.
uint32_t INTC_GetPendingIRQ (IRQn_Type IRQn);

// Set a device specific interrupt to pending.
void INTC_SetPendingIRQ (IRQn_Type IRQn);

// Clear a device specific interrupt from pending
void INTC_ClearPendingIRQ (IRQn_Type IRQn);

// Get the device specific interrupt active status
uint32_t INTC_GetActive (IRQn_Type IRQn);

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
void* INTC_GetVector (IRQn_Type IRQn);

// Modify Interrupt Vector 
void INTC_SetVector (IRQn_Type IRQn, void* vector);

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

  
#endif
