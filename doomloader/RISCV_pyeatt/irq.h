/* irq.h - Interrupt control routines for the RISC-V Pyeatt core (M-mode).
 *
 * Prototypes for the helpers defined in irq.S.  The masks below match
 * the mip/mie bit layout described in the CSR Register File chapter of
 * the RISC-V Pyeatt Architecture Reference Manual.
 */

#ifndef RISCV_PYEATT_IRQ_H
#define RISCV_PYEATT_IRQ_H

#include <stdint.h>

/* Standard M-mode interrupt bits in mie / mip. */
#define IRQ_MSI_MASK    (1u << 3)                /* Machine software    */
#define IRQ_MTI_MASK    (1u << 7)                /* Machine timer       */
#define IRQ_MEI_MASK    (1u << 11)               /* Machine external    */

/* Platform interrupt n (0..IRQ_WIDTH-1) occupies mie/mip bit 16+n. */
#define IRQ_PLATFORM_MASK(n)    (1u << (16 + (n)))

/* Globally enable M-mode interrupts (sets mstatus.MIE). */
void     irq_global_enable(void);

/* Globally disable M-mode interrupts (clears mstatus.MIE).
 * Returns the previous MIE value, 0 or 1, for later restoration. */
uint32_t irq_global_disable(void);

/* Restore mstatus.MIE to the value returned by irq_global_disable. */
void     irq_global_restore(uint32_t prev);

/* Return the current value of mstatus.MIE (0 or 1). */
uint32_t irq_global_status(void);

/* Enable the interrupt sources whose bits are set in mask (ORs into mie). */
void     irq_enable(uint32_t mask);

/* Disable the interrupt sources whose bits are set in mask (clears mie). */
void     irq_disable(uint32_t mask);

/* Return the contents of mie (currently enabled sources). */
uint32_t irq_enabled_mask(void);

/* Return the contents of mip (currently pending sources).  The standard
 * and platform bits are read-only passthroughs reflecting the level on
 * the corresponding input port.  Clear the source device (MTIMER,
 * MSIP word, PLIC, UART, etc.) to clear a pending bit. */
uint32_t irq_pending_mask(void);

#endif /* RISCV_PYEATT_IRQ_H */
