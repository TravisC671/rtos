//------------------------------------------------------------------------------
// Copyright (c) 2026 Larry D. Pyeatt
// All rights reserved.
//------------------------------------------------------------------------------
// asm_string.h - Cortex-M3 assembly string/memory function declarations
//
// This header declares the hand-written Thumb-2 replacements for the C
// library <string.h> functions memset and memcpy, plus two fast-path
// wrappers optimised for VGA framebuffer operations.
//
// Include this header instead of <string.h>.  The assembly implementations
// (memset_fast.S, memcpy_fast.S) are linked directly into the binary and
// override the library versions automatically.
//
// Source files:
//   src/memset_fast.S  -- memset, vga_clear_buffer_asm
//   src/memcpy_fast.S  -- memcpy, vga_copy_buffer_asm
//------------------------------------------------------------------------------

#ifndef ASM_STRING_H
#define ASM_STRING_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// memset - fill memory with a constant byte
//
// Replaces the C library memset.  Uses STMIA (Store Multiple, Increment After)
// to write 32 bytes per iteration when dest is word-aligned and n >= 32,
// generating an 8-word AXI burst on every loop cycle.  Handles arbitrary
// alignment and any byte count correctly via byte head/tail paths.
//
// Parameters:
//   dest  - destination pointer (any alignment)
//   c     - fill byte (passed as int per C standard; only bits 7:0 used)
//   n     - number of bytes to fill
// Returns:
//   dest (unchanged)
//------------------------------------------------------------------------------
void *memset(void *dest, int c, size_t n);

//------------------------------------------------------------------------------
// memcpy - copy n bytes from src to dest  (non-overlapping regions only)
//
// Replaces the C library memcpy.  When both dest and src are word-aligned
// uses paired LDMIA + STMIA to copy 32 bytes per iteration, issuing an
// 8-word AXI read burst followed by an 8-word AXI write burst.  Falls back
// to a byte loop when src is misaligned after dest alignment is established.
//
// Parameters:
//   dest  - destination pointer (any alignment)
//   src   - source pointer (any alignment)
//   n     - number of bytes to copy
// Returns:
//   dest (unchanged)
//
// Note: behaviour is undefined if [dest, dest+n) and [src, src+n) overlap.
//       Use memmove (not provided here) for overlapping regions.
//------------------------------------------------------------------------------
void *memcpy(void *dest, const void *src, size_t n);

//------------------------------------------------------------------------------
// vga_clear_buffer_asm - fast aligned fill for VGA framebuffers
//
// Optimised wrapper around the STMIA burst loop.  Skips all alignment and
// tail-handling overhead.  Goes straight into the 32-bytes-per-iteration
// burst loop for the entire buffer.
//
// Requirements (caller's responsibility):
//   dest  - 4-byte aligned  (VGA_FRAMEBUFFER_BASE and pvPortMalloc satisfy this)
//   n     - non-zero multiple of 32
//             320x200 buffer: 64,000  = 2,000 x 32
//             640x400 buffer: 256,000 = 8,000 x 32
//
// Parameters:
//   dest   - 4-byte aligned destination pointer
//   color  - palette index to fill (8-bit; replicated to 32 bits internally)
//   n      - byte count (must be a non-zero multiple of 32)
//------------------------------------------------------------------------------
void vga_clear_buffer_asm(void *dest, uint8_t color, uint32_t n);

//------------------------------------------------------------------------------
// vga_copy_buffer_asm - fast aligned copy for VGA framebuffers
//
// Optimised wrapper around the paired LDMIA+STMIA burst loop.  Used by
// vga_update_task to copy the software framebuffer in RAM to the VGA
// controller's hardware framebuffer over AXI.
//
// Each iteration issues eight consecutive AXI read transactions (LDMIA)
// followed immediately by eight consecutive AXI write transactions (STMIA),
// keeping both directions of the bus busy back-to-back.
//
// Requirements (caller's responsibility):
//   dest  - 4-byte aligned  (VGA_FRAMEBUFFER_BASE is always aligned)
//   src   - 4-byte aligned  (pvPortMalloc aligns to at least 4 bytes)
//   n     - non-zero multiple of 32
//             640x400 buffer: 256,000 = 8,000 x 32
//
// Parameters:
//   dest  - 4-byte aligned destination pointer
//   src   - 4-byte aligned source pointer
//   n     - byte count (must be a non-zero multiple of 32)
//------------------------------------------------------------------------------
void vga_copy_buffer_asm(void *dest, const void *src, uint32_t n);

#ifdef __cplusplus
}
#endif

#endif // ASM_STRING_H
