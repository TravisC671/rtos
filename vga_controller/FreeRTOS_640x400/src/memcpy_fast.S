// memcpy_fast.S - Cortex-M3 memcpy replacement
// Copyright (c) 2026 Larry D. Pyeatt.  All rights reserved.
//
// void *memcpy(void *dest, const void *src, size_t n);
//
// AAPCS entry:
//   r0  dest  - destination pointer (returned unchanged)
//   r1  src   - source pointer
//   r2  n     - byte count
//
// Strategy
// ---------
// When both dest and src are 4-byte aligned the inner loop pairs LDMIA and
// STMIA to load and store 8 words (32 bytes) per iteration.  On an AXI
// interconnect LDMIA issues eight back-to-back read transactions followed
// immediately by STMIA issuing eight back-to-back write transactions,
// maximising bus utilisation in both directions.
//
// For the vga_copy_buffer_asm fast path (used by vga_update_task to copy
// the software framebuffer to the VGA hardware) both pointers are always
// word-aligned and n is always a multiple of 32, so it goes straight into
// the burst loop with no overhead.
//
// General-purpose memcpy handling:
//   1. Early exit for n == 0.
//   2. Head: copy bytes one at a time until dest is 4-byte aligned (0..3 bytes).
//   3. Alignment check: if src is also word-aligned after the head, use the
//      32-byte burst body; otherwise fall back to a byte-copy loop (correct
//      but slower -- rare in practice since allocators align to at least 4 bytes).
//   4. Word tail: copy remaining complete 4-byte words.
//   5. Byte tail: copy remaining 0..3 bytes.
//
// Register map (after prologue):
//   r0   dest ptr, advanced as bytes are copied (original saved in r12)
//   r1   src ptr,  advanced in step with dest
//   r2   remaining byte count
//   r3   scratch: head count, block count, tail count, alignment test
//   r4-r11  eight data words for LDMIA/STMIA burst
//
// Calling convention:
//   r0-r3  caller-saved; freely overwritten
//   r4-r11 callee-saved; pushed on entry, popped on exit
//   r12    caller-saved; used to preserve original r0 for return value
//
// Assembled with -mthumb -mcpu=cortex-m3 (.syntax unified, Thumb-2).

        .syntax unified
        .cpu    cortex-m3
        .thumb

        .section .text.memcpy, "ax", %progbits
        .align  2                           // 4-byte alignment required for Thumb-2

        .global memcpy
        .type   memcpy, %function

memcpy:
        // ----------------------------------------------------------------
        // Prologue: save callee-saved regs, preserve dest in r12.
        // ----------------------------------------------------------------
        push    {r4-r11}
        mov     r12, r0                     // save original dest for return

        // ----------------------------------------------------------------
        // Early exit: nothing to do when n == 0.
        // ----------------------------------------------------------------
        cbz     r2, .Lcpy_done

        // ----------------------------------------------------------------
        // Head: copy bytes until dest is 4-byte aligned (0..3 bytes).
        // Using bytes here avoids writing past any alignment boundary and
        // is only ever 0..3 iterations.
        // ----------------------------------------------------------------
        ands    r3, r0, #3                  // r3 = dest & 3
        beq     .Lcpy_check_src             // already aligned
        rsb     r3, r3, #4                  // r3 = bytes needed to align dest
        cmp     r3, r2                      // clamp to remaining count
        it      gt
        movgt   r3, r2

.Lcpy_head_loop:
        ldrb    r4, [r1], #1               // load one byte from src
        strb    r4, [r0], #1               // store one byte to dest
        subs    r3, r3, #1
        subs    r2, r2, #1
        bne     .Lcpy_head_loop

        // ----------------------------------------------------------------
        // Alignment check: dest is now word-aligned.
        // Enter the burst loop only if src is also word-aligned.
        // If src is misaligned fall through to the byte copy loop.
        // ----------------------------------------------------------------
.Lcpy_check_src:
        cbz     r2, .Lcpy_done              // nothing left after head
        tst     r1, #3                      // test src alignment (sets Z if aligned)
        bne     .Lcpy_slow_path             // src unaligned: use byte loop

        // ----------------------------------------------------------------
        // Main burst loop: 32 bytes (8 words) per iteration.
        // LDMIA loads  r4-r11 from [r1], advances r1 by 32.
        // STMIA stores r4-r11 to  [r0], advances r0 by 32.
        // On an AXI bus this issues eight consecutive reads then eight
        // consecutive writes -- a read burst followed by a write burst.
        // ----------------------------------------------------------------
        lsrs    r3, r2, #5                  // r3 = n / 32 (full blocks)
        beq     .Lcpy_word_tail             // fewer than 32 bytes remain

.Lcpy_burst_loop:
        ldmia   r1!, {r4-r11}              // burst-load  32 bytes from src
        stmia   r0!, {r4-r11}              // burst-store 32 bytes to dest
        subs    r3, r3, #1
        bne     .Lcpy_burst_loop

        and     r2, r2, #31                // r2 = n % 32  (0..31 bytes left)

        // ----------------------------------------------------------------
        // Word tail: copy remaining complete 4-byte words (0..7).
        // ----------------------------------------------------------------
.Lcpy_word_tail:
        lsrs    r3, r2, #2                  // r3 = remaining / 4
        beq     .Lcpy_byte_tail

.Lcpy_word_tail_loop:
        ldr     r4, [r1], #4
        str     r4, [r0], #4
        subs    r3, r3, #1
        bne     .Lcpy_word_tail_loop

        and     r2, r2, #3                 // r2 = remaining % 4  (0..3 bytes)

        // ----------------------------------------------------------------
        // Byte tail: copy remaining 0..3 bytes.
        // ----------------------------------------------------------------
.Lcpy_byte_tail:
        cbz     r2, .Lcpy_done

.Lcpy_byte_tail_loop:
        ldrb    r4, [r1], #1
        strb    r4, [r0], #1
        subs    r2, r2, #1
        bne     .Lcpy_byte_tail_loop
        b       .Lcpy_done

        // ----------------------------------------------------------------
        // Slow path: src is not word-aligned after the head.
        // Copy every remaining byte individually.  Correct for all inputs;
        // slower than the burst loop but only reached when src is
        // misaligned -- uncommon with standard allocators.
        // ----------------------------------------------------------------
.Lcpy_slow_path:
.Lcpy_slow_loop:
        ldrb    r4, [r1], #1
        strb    r4, [r0], #1
        subs    r2, r2, #1
        bne     .Lcpy_slow_loop

        // ----------------------------------------------------------------
        // Epilogue: restore callee-saved registers, return original dest.
        // ----------------------------------------------------------------
.Lcpy_done:
        mov     r0, r12                    // return value = original dest
        pop     {r4-r11}
        bx      lr

        .size   memcpy, . - memcpy

// ---------------------------------------------------------------------------
// vga_copy_buffer_asm - fast aligned buffer copy using LDMIA+STMIA bursts
//
// void vga_copy_buffer_asm(void *dest, const void *src, uint32_t n);
//
// Optimised entry point for vga_update_task.  Skips all alignment checks
// and tail handling; requires:
//   dest  - 4-byte aligned  (VGA_FRAMEBUFFER_BASE is always aligned)
//   src   - 4-byte aligned  (video_buffer from pvPortMalloc is aligned)
//   n     - multiple of 32  (256,000 = 8,000x32 for 640x400)
//
// Each iteration pairs LDMIA and STMIA to issue eight AXI read transactions
// immediately followed by eight AXI write transactions, keeping both the
// read and write data buses busy back-to-back.
//
// AAPCS entry:
//   r0  dest  - 4-byte aligned destination (not returned)
//   r1  src   - 4-byte aligned source
//   r2  n     - byte count, must be a non-zero multiple of 32
// ---------------------------------------------------------------------------

        .section .text.vga_copy_buffer_asm, "ax", %progbits
        .align  2

        .global vga_copy_buffer_asm
        .type   vga_copy_buffer_asm, %function

vga_copy_buffer_asm:
        push    {r4-r11}

        // Convert byte count to 32-byte block count.
        // Caller guarantees n is a non-zero multiple of 32.
        lsr     r2, r2, #5                  // r2 = n / 32

.Lvcb_copy_burst_loop:
        // Load 32 bytes from src (8-word AXI read burst), then
        // store 32 bytes to dest (8-word AXI write burst).
        ldmia   r1!, {r4-r11}
        stmia   r0!, {r4-r11}
        subs    r2, r2, #1
        bne     .Lvcb_copy_burst_loop

        pop     {r4-r11}
        bx      lr

        .size   vga_copy_buffer_asm, . - vga_copy_buffer_asm

        .end
