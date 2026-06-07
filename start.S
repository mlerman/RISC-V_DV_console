/*
 * ============================================================================
 * RISC-V DV Console by Mikhael Lerman https://checkthisresume.com
 * ============================================================================
 * Copyright (c) 2026 Mikhael Lerman (https://checkthisresume.com)
 *
 * This software is released under the terms of the MIT License.
 * The above copyright notice, banner, and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND...
 */
.section .text.start
.global _start

_start:
    /* --- Multi-hart initialization fix --- */
    csrr t0, mhartid
    bnez t0, hang_secondary  /* If hartid != 0, jump to hang loop */
    /* ------------------------------------- */

    .option push
    .option norelax
    la sp, stack_top
    .option pop

    .option push
    .option norelax
    la t0, m_trap_entry
    .option pop
    csrw mtvec, t0

    li t0, 0x3ffffffffffff
    csrw pmpaddr0, t0
    li t0, 0x1f
    csrw pmpcfg0, t0

    li t0, (1 << 11)
    csrw mie, t0

    li t0, (1 << 3)
    csrw mstatus, t0

    /* mark hart 0 as ACTIVE in shared state */
    .option push
    .option norelax
    la t0, hart_state
    .option pop
    li t1, 1          /* HART_ACTIVE = 1 */
    sw t1, 0(t0)      /* hart_state[0] = HART_ACTIVE */

    /* hart 0 marks itself present */
    .option push
    .option norelax
    la t0, hart_present
    .option pop
    li t1, 1
    sw t1, 0(t0)      /* hart_present[0] = 1 */

    call main
hang:
    j hang

hang_secondary:
    /*
     * Generic secondary hart startup (harts 1, 2, 3).
     * Load per-hart stack top from a table indexed by mhartid.
     * All secondary harts share one trap entry and park loop.
     */
    csrr t0, mhartid

    /* load stack top: stack_tops[mhartid] */
    .option push
    .option norelax
    la t1, secondary_stack_tops
    .option pop
    slli t2, t0, 3        /* index * 8 (pointer size) */
    add t1, t1, t2
    ld sp, 0(t1)          /* sp = secondary_stack_tops[hartid] */

    /* configure PMP — required for S-mode memory access.
     * PMP is per-hart; secondary harts must configure it
     * independently or S-mode instruction fetches will fault. */
    li t0, 0x3ffffffffffff
    csrw pmpaddr0, t0
    li t0, 0x1f
    csrw pmpcfg0, t0

    /* install generic secondary trap handler */
    .option push
    .option norelax
    la t0, secondary_trap_entry
    .option pop
    csrw mtvec, t0

    /* enable M-mode software interrupts so IPI can wake us */
    li t0, (1 << 3)   /* mie.MSIE */
    csrw mie, t0
    li t0, (1 << 3)   /* mstatus.MIE */
    csrw mstatus, t0

    /* mark this hart as PARKED: hart_state[mhartid] = 0 */
    csrr t0, mhartid
    .option push
    .option norelax
    la t1, hart_state
    .option pop
    slli t2, t0, 2        /* index * 4 (int size) */
    add t1, t1, t2
    sw zero, 0(t1)        /* hart_state[hartid] = HART_PARKED */

    /* signal hart 0 we are present */
    csrr t0, mhartid
    .option push
    .option norelax
    la t1, hart_present
    .option pop
    slli t2, t0, 2        /* index * 4 (unsigned int size) */
    add t1, t1, t2
    li t2, 1
    sw t2, 0(t1)          /* hart_present[mhartid] = 1 */


.global park
park:
    wfi
    j park

/*
 * secondary_stack_tops — table of stack top pointers for harts 1..3.
 * Index 0 unused (hart 0 uses stack_top directly).
 * Placed in .data so it's writable (though we only read it here).
 */
.section .rodata
.global secondary_stack_tops
secondary_stack_tops:
    .dword 0            /* hart 0 — unused, placeholder */
    .dword stack1_top   /* hart 1 */
    .dword stack2_top   /* hart 2 */
    .dword stack3_top   /* hart 3 */
    .dword stack4_top   /* hart 4 */
    .dword stack5_top   /* hart 5 */
    .dword stack6_top   /* hart 6 */
    .dword stack7_top   /* hart 7 */
    .dword stack8_top   /* hart 8 */
    .dword stack9_top   /* hart 9 */
    .dword stack10_top   /* hart 10 */
    .dword stack11_top   /* hart 11 */
    .dword stack12_top   /* hart 12 */
    .dword stack13_top   /* hart 13 */
    .dword stack14_top   /* hart 14 */
    .dword stack15_top   /* hart 15 */
.section .text

/* ── generic secondary hart trap entry (harts 1, 2, 3) ─────────── */
.section .text
.global secondary_trap_entry
.align 4
secondary_trap_entry:
    addi sp, sp, -128
    sd ra,  0(sp)
    sd t0,  8(sp)
    sd t1,  16(sp)
    sd t2,  24(sp)
    sd a0,  32(sp)
    sd a1,  40(sp)
    sd a2,  48(sp)
    sd a3,  56(sp)
    sd a4,  64(sp)
    sd a5,  72(sp)
    sd a6,  80(sp)
    sd a7,  88(sp)
    sd t3,  96(sp)
    sd t4,  104(sp)
    sd t5,  112(sp)
    sd t6,  120(sp)

    call secondary_trap_handler

    ld ra,  0(sp)
    ld t0,  8(sp)
    ld t1,  16(sp)
    ld t2,  24(sp)
    ld a0,  32(sp)
    ld a1,  40(sp)
    ld a2,  48(sp)
    ld a3,  56(sp)
    ld a4,  64(sp)
    ld a5,  72(sp)
    ld a6,  80(sp)
    ld a7,  88(sp)
    ld t3,  96(sp)
    ld t4,  104(sp)
    ld t5,  112(sp)
    ld t6,  120(sp)
    addi sp, sp, 128
    mret

.section .text
.global m_trap_entry
.align 4
m_trap_entry:
    addi sp, sp, -128
    sd ra,  0(sp)
    sd t0,  8(sp)
    sd t1,  16(sp)
    sd t2,  24(sp)
    sd a0,  32(sp)
    sd a1,  40(sp)
    sd a2,  48(sp)
    sd a3,  56(sp)
    sd a4,  64(sp)
    sd a5,  72(sp)
    sd a6,  80(sp)
    sd a7,  88(sp)
    sd t3,  96(sp)
    sd t4,  104(sp)
    sd t5,  112(sp)
    sd t6,  120(sp)

    call m_trap_handler

    ld ra,  0(sp)
    ld t0,  8(sp)
    ld t1,  16(sp)
    ld t2,  24(sp)
    ld a0,  32(sp)
    ld a1,  40(sp)
    ld a2,  48(sp)
    ld a3,  56(sp)
    ld a4,  64(sp)
    ld a5,  72(sp)
    ld a6,  80(sp)
    ld a7,  88(sp)
    ld t3,  96(sp)
    ld t4,  104(sp)
    ld t5,  112(sp)
    ld t6,  120(sp)
    addi sp, sp, 128
    mret

.section .text
.global s_trap_entry
.align 4
s_trap_entry:
    addi sp, sp, -128
    sd ra,  0(sp)
    sd t0,  8(sp)
    sd t1,  16(sp)
    sd t2,  24(sp)
    sd a0,  32(sp)
    sd a1,  40(sp)
    sd a2,  48(sp)
    sd a3,  56(sp)
    sd a4,  64(sp)
    sd a5,  72(sp)
    sd a6,  80(sp)
    sd a7,  88(sp)
    sd t3,  96(sp)
    sd t4,  104(sp)
    sd t5,  112(sp)
    sd t6,  120(sp)

    call s_trap_handler

    ld ra,  0(sp)
    ld t0,  8(sp)
    ld t1,  16(sp)
    ld t2,  24(sp)
    ld a0,  32(sp)
    ld a1,  40(sp)
    ld a2,  48(sp)
    ld a3,  56(sp)
    ld a4,  64(sp)
    ld a5,  72(sp)
    ld a6,  80(sp)
    ld a7,  88(sp)
    ld t3,  96(sp)
    ld t4,  104(sp)
    ld t5,  112(sp)
    ld t6,  120(sp)
    addi sp, sp, 128
    sret

.section .text
.global switch_to_smode
switch_to_smode:
    li t0, 0xffffffff
    csrw mideleg, t0
    csrw medeleg, zero

    .option push
    .option norelax
    la t0, s_trap_entry
    .option pop
    csrw stvec, t0

    li t0, (1 << 9)
    csrw sie, t0

    li t0, (1 << 1)
    csrw sstatus, t0

    li t0, (3 << 11)
    csrc mstatus, t0
    li t0, (1 << 11)
    csrs mstatus, t0

    csrw mepc, a0

    /* per-hart stack — hart 0 uses stack_top (v2ok path),
     * others use secondary_stack_tops table */
    csrr t0, mhartid
    beqz t0, 1f
    .option push
    .option norelax
    la t1, secondary_stack_tops
    .option pop
    slli t2, t0, 3        /* index * 8 */
    add t1, t1, t2
    ld sp, 0(t1)
    j 2f
1:
    .option push
    .option norelax
    la sp, stack_top
    .option pop
2:
    mret

.section .text
.global switch_to_mmode
switch_to_mmode:
    csrw mideleg, zero
    csrw medeleg, zero

    .option push
    .option norelax
    la t0, m_trap_entry
    .option pop
    csrw mtvec, t0

    li t0, (1 << 11)
    csrw mie, t0

    li t0, (3 << 11)
    csrs mstatus, t0

    li t0, (1 << 3)
    csrs mstatus, t0

    csrw mepc, a0

    .option push
    .option norelax
    la sp, stack_top
    .option pop

    mret
/* ── hart 0 park trap entry (used during hartswitch!) ───────────────── */
.section .text
.global hart0_trap_entry
.align 4
hart0_trap_entry:
    addi sp, sp, -128
    sd ra,  0(sp)
    sd t0,  8(sp)
    sd t1,  16(sp)
    sd t2,  24(sp)
    sd a0,  32(sp)
    sd a1,  40(sp)
    sd a2,  48(sp)
    sd a3,  56(sp)
    sd a4,  64(sp)
    sd a5,  72(sp)
    sd a6,  80(sp)
    sd a7,  88(sp)
    sd t3,  96(sp)
    sd t4,  104(sp)
    sd t5,  112(sp)
    sd t6,  120(sp)

    call hart0_trap_handler

    ld ra,  0(sp)
    ld t0,  8(sp)
    ld t1,  16(sp)
    ld t2,  24(sp)
    ld a0,  32(sp)
    ld a1,  40(sp)
    ld a2,  48(sp)
    ld a3,  56(sp)
    ld a4,  64(sp)
    ld a5,  72(sp)
    ld a6,  80(sp)
    ld a7,  88(sp)
    ld t3,  96(sp)
    ld t4,  104(sp)
    ld t5,  112(sp)
    ld t6,  120(sp)
    addi sp, sp, 128
    mret

/* ── park0: hart 0 parking loop (used during hartswitch!) ───────────── */
.global park0
park0:
    wfi
    j park0
