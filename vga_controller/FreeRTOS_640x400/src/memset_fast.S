// memset_fast.S - Cortex-M3 memset replacement
// Copyright (c) 2026 Larry D. Pyeatt.  All rights reserved.
//
// void *memset(void *dest, int c, size_t n);
//
// AAPCS entry:
//   r0  dest  - pointer to fill region (returned unchanged)
//   r1  c     - fill byte (only bits 7:0 are significant)
//   r2  n     - byte count
//
// Strategy
// ---------
// For the common framebuffer case dest is always word-aligned and n is
// always a multiple of 32, so the inner loop issues STMIA instructions
// that store 8 consecutive words (32 bytes) per iteration.  On an AXI
// interconnect this causes eight back-to-back 32-bit bus transactions --
// a burst -- dramatically faster than byte-by-byte stores.
//
// To be a correct general-purpose replacement for the C library memset
// the function also handles:
//   - unaligned destination pointers (byte head to reach word alignment)
//   - counts that are not multiples of 32 (word tail, then byte tail)
//   - count == 0 (early exit)
//
// Register map (after prologue):
//   r0   dest ptr, advanced as bytes are stored (original saved in r12)
//   r1   fill word (fill byte replicated into all four bytes)
//   r2   remaining byte count
//   r3   scratch: head byte count, block count, tail word count
//   r4-r11  eight copies of the fill word for STMIA burst
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

        .section .text.memset, "ax", %progbits
        .align  2                           // 4-byte alignment required for Thumb-2

        .global memset
        .type   memset, %function

memset:
        // ----------------------------------------------------------------
        // Prologue: save callee-saved regs, preserve dest in r12.
        // r4-r11 hold the eight burst words; r12 holds the return value.
        // ----------------------------------------------------------------
        push    {r4-r11}                    // save callee-saved registers
        mov     r12, r0                     // save original dest for return

        // ----------------------------------------------------------------
        // Early exit: nothing to do when n == 0.
        // ----------------------------------------------------------------
        cbz     r2, .Lms_done

        // ----------------------------------------------------------------
        // Byte splat: replicate the fill byte into every byte of r1.
        //   r1 = 0x000000CC
        //   r1 = 0x0000CCCC  (after shift-or by 8)
        //   r1 = 0xCCCCCCCC  (after shift-or by 16)
        // ----------------------------------------------------------------
        uxtb    r1, r1                      // mask to 8 bits
        orr     r1, r1, r1, lsl #8
        orr     r1, r1, r1, lsl #16

        // ----------------------------------------------------------------
        // Head: copy bytes one at a time until dest is 4-byte aligned.
        // Bytes needed = (4 - (dest & 3)) & 3  =>  0, 1, 2, or 3.
        // ----------------------------------------------------------------
        ands    r3, r0, #3                  // r3 = dest & 3
        beq     .Lms_aligned                // already aligned
        rsb     r3, r3, #4                  // r3 = bytes to alignment boundary
        cmp     r3, r2                      // clamp to remaining count
        it      gt
        movgt   r3, r2

.Lms_head_loop:
        strb    r1, [r0], #1               // store one byte, advance dest
        subs    r3, r3, #1
        subs    r2, r2, #1
        bne     .Lms_head_loop

        // ----------------------------------------------------------------
        // Main burst loop: 32 bytes (8 words) per iteration via STMIA.
        // Load r4-r11 with the fill word once; loop writes them to dest.
        // ----------------------------------------------------------------
.Lms_aligned:
        cbz     r2, .Lms_done

        mov     r4,  r1                     // load eight burst registers
        mov     r5,  r1
        mov     r6,  r1
        mov     r7,  r1
        mov     r8,  r1
        mov     r9,  r1
        mov     r10, r1
        mov     r11, r1

        lsrs    r3, r2, #5                  // r3 = n / 32 (full blocks)
        beq     .Lms_word_tail              // fewer than 32 bytes remain

.Lms_burst_loop:
        // STMIA stores r4-r11 (8 x 4 = 32 bytes) to [r0], advances r0 by 32.
        // On an AXI bus this issues as eight back-to-back 32-bit writes (burst).
        stmia   r0!, {r4-r11}
        subs    r3, r3, #1
        bne     .Lms_burst_loop

        and     r2, r2, #31                // r2 = n % 32  (0..31 bytes left)

        // ----------------------------------------------------------------
        // Word tail: store remaining complete words (0..7).
        // ----------------------------------------------------------------
.Lms_word_tail:
        lsrs    r3, r2, #2                  // r3 = remaining / 4
        beq     .Lms_byte_tail

.Lms_word_tail_loop:
        str     r1, [r0], #4
        subs    r3, r3, #1
        bne     .Lms_word_tail_loop

        and     r2, r2, #3                 // r2 = remaining % 4  (0..3 bytes)

        // ----------------------------------------------------------------
        // Byte tail: store remaining 0..3 bytes.
        // ----------------------------------------------------------------
.Lms_byte_tail:
        cbz     r2, .Lms_done

.Lms_byte_tail_loop:
        strb    r1, [r0], #1
        subs    r2, r2, #1
        bne     .Lms_byte_tail_loop

        // ----------------------------------------------------------------
        // Epilogue: restore callee-saved registers, return original dest.
        // ----------------------------------------------------------------
.Lms_done:
        mov     r0, r12                    // return value = original dest
        pop     {r4-r11}
        bx      lr

        .size   memset, . - memset

// ---------------------------------------------------------------------------
// vga_clear_buffer_asm - fast aligned buffer fill using STMIA burst writes
//
// void vga_clear_buffer_asm(void *dest, uint8_t color, uint32_t n);
//
// Optimised entry point for clear_buffer().  Skips alignment probing and
// tail handling; requires:
//   dest  - 4-byte aligned  (VGA_FRAMEBUFFER_BASE and pvPortMalloc guarantee this)
//   n     - multiple of 32  (64,000 = 2,000x32 for 320x200;
//                            256,000 = 8,000x32 for 640x400)
//
// AAPCS entry:
//   r0  dest   - 4-byte aligned destination (not returned)
//   r1  color  - fill byte (bits 7:0)
//   r2  n      - byte count, must be a non-zero multiple of 32
// ---------------------------------------------------------------------------

        .section .text.vga_clear_buffer_asm, "ax", %progbits
        .align  2

        .global vga_clear_buffer_asm
        .type   vga_clear_buffer_asm, %function

vga_clear_buffer_asm:
        push    {r4-r11}

        // Byte splat: replicate color into every byte of a 32-bit word.
        uxtb    r1, r1
        orr     r1, r1, r1, lsl #8         // 0x0000CCCC
        orr     r1, r1, r1, lsl #16        // 0xCCCCCCCC

        // Load all eight burst registers with the fill word.
        mov     r4,  r1
        mov     r5,  r1
        mov     r6,  r1
        mov     r7,  r1
        mov     r8,  r1
        mov     r9,  r1
        mov     r10, r1
        mov     r11, r1

        // Convert byte count to 32-byte block count.
        // Caller guarantees n is a non-zero multiple of 32.
        lsr     r2, r2, #5                  // r2 = n / 32

.Lvcb_burst_loop:
        // STMIA: store 32 bytes to [r0] and advance r0 by 32.
        // Generates an 8-word AXI burst on every iteration.
        stmia   r0!, {r4-r11}
        subs    r2, r2, #1
        bne     .Lvcb_burst_loop

        pop     {r4-r11}
        bx      lr

        .size   vga_clear_buffer_asm, . - vga_clear_buffer_asm

        .end
