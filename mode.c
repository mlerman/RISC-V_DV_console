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
#include "smp.h"
#define MODE_M 0
#define MODE_S 1

volatile int current_mode = MODE_M;
volatile int ecall_pending = 0;

void aplic_m_init();
void aplic_s_init();
void uart_puts(const char *s);
void uart_putc(char c);

void switch_to_smode(void *entry);
void switch_to_mmode(void *entry);

void s_mode_loop();
void m_mode_loop();

void do_smode() {
    uart_puts("Switching to S-mode...\r\n");
    current_mode = MODE_S;
    aplic_s_init();
    switch_to_smode(s_mode_loop);
}

void do_mmode() {
    uart_puts("Switching to M-mode...\r\n");
    current_mode = MODE_M;
    aplic_m_init();
    switch_to_mmode(m_mode_loop);
}

void do_whoami() {
    if (current_mode == MODE_M)
        uart_puts("Current mode: M-mode\r\n");
    else
        uart_puts("Current mode: S-mode\r\n");
}

/*
 * whoami? — hardware probe version.
 *
 * There is no CSR that directly exposes the current privilege level.
 * Instead we exploit the fact that mhartid is M-mode only: reading it
 * from S-mode raises an illegal-instruction exception (mcause = 2).
 * m_trap_handler catches that, skips the faulting instruction (+4),
 * and sets hw_probe_fault = 1.  No fault means we are in M-mode.
 */
void do_whoami_hw() {
    extern volatile int hw_probe_fault;
    hw_probe_fault = 0;

    unsigned long val;
    __asm__ volatile ("csrr %0, mhartid" : "=r"(val));

    if (hw_probe_fault)
        uart_puts("HW probe: S-mode (mhartid read trapped)\r\n");
    else
        uart_puts("HW probe: M-mode (mhartid read succeeded)\r\n");
}

void do_harts() {
    unsigned long current_hart;
    __asm__ volatile ("csrr %0, mhartid" : "=r"(current_hart));

    uart_puts("\r\n--- System Hart Overview ---\r\n");
    uart_puts("   +--------+----------+\r\n");
    uart_puts("   | HartID |  Status  |\r\n");
    uart_puts("   +--------+----------+\r\n");

    for (int i = 0; i < (int)num_harts; i++) {
        uart_puts("   |   ");
        uart_putc('0' + i);
        uart_puts("    | ");
        if (hart_state[i] == HART_ACTIVE)
            uart_puts("ACTIVE   |\r\n");
        else
            uart_puts("PARKED   |\r\n");
    }
    uart_puts("   +--------+----------+\r\n\r\n");

    uart_puts("Active Hart Details:\r\n");
    uart_puts("Current Hart ID: ");
    uart_putc('0' + (char)console_hartid);
    uart_puts("\r\n");

    if (current_mode == MODE_M) {
        unsigned long misa_val;
        __asm__ volatile ("csrr %0, misa" : "=r"(misa_val));

        unsigned char xlen = (misa_val >> 62) & 0x3;
        uart_puts("Base Arch: ");
        if (xlen == 1)      uart_puts("RV32\r\n");
        else if (xlen == 2) uart_puts("RV64\r\n");
        else                uart_puts("Unknown\r\n");

        uart_puts("Extensions: ");
        for (int i = 0; i < 26; i++) {
            if (misa_val & (1UL << i)) {
                uart_putc('A' + i);
                uart_putc(' ');
            }
        }
        uart_puts("\r\n");
    } else {
        uart_puts("ISA Extensions: [Hidden] (Switch to M-mode to read misa)\r\n");
    }
    uart_puts("----------------------------\r\n");
}
/* ── nexth! ────────────────────────────────────────────────────── */


void aplic_retarget_idc(int hart);
void m_mode_loop();

#define CLINT_MSIP(h) (*((volatile unsigned int *)(0x02000000UL + 4*(h))))

void do_nexth() {
    unsigned long my_hart;
    __asm__ volatile ("csrr %0, mhartid" : "=r"(my_hart));

    int target = ((int)my_hart + 1) % (int)num_harts;   /* cycle based on detected hart count */

    uart_puts("Switching console to hart ");
    uart_putc('0' + target);
    uart_puts("...\r\n");

    /* 1. set rendezvous entry for waking hart */
    hart_entry = m_mode_loop;

    /* 2. mark ourselves parked */
    hart_state[my_hart] = HART_PARKED;

    /* 3. install park trap handler.
     * Hart 0 keeps MEIE enabled so it can forward UART RX to
     * the shared rx_buf while any other hart owns the console.
     * Secondary harts use generic secondary_trap_entry (MSIE only). */
    unsigned long mtvec_val;
    if (my_hart == 0) {
        __asm__ volatile (
            ".option push;"
            ".option norelax;"
            "la %0, hart0_trap_entry;"
            ".option pop;"
            : "=r"(mtvec_val)
        );
        __asm__ volatile ("csrw mtvec, %0" :: "r"(mtvec_val));
        /* keep MSIE + MEIE enabled — forward UART RX while parked */
        __asm__ volatile ("li t0, ((1<<11)|(1<<3)); csrw mie, t0" ::: "t0");
        __asm__ volatile ("li t0, (1 << 3); csrw mstatus, t0" ::: "t0");
    } else {
        __asm__ volatile (
            ".option push;"
            ".option norelax;"
            "la %0, secondary_trap_entry;"
            ".option pop;"
            : "=r"(mtvec_val)
        );
        __asm__ volatile ("csrw mtvec, %0" :: "r"(mtvec_val));
        /* MSIE only — secondary harts don't handle UART when parked */
        __asm__ volatile ("li t0, (1 << 3); csrw mie, t0" ::: "t0");
        __asm__ volatile ("li t0, (1 << 3); csrw mstatus, t0" ::: "t0");
    }

    /* 5. update console_hartid so target hart's prompt is correct */
    console_hartid = (unsigned long)target;

    /* 6. fire IPI to wake target */
    CLINT_MSIP(target) = 1;

    /* 6. park — jump to wfi loop, woken by future nexth! IPI */
    if (my_hart == 0)
        __asm__ volatile ("j park0");
    else
        __asm__ volatile ("j park");   /* all secondary harts share park loop */

    /* never reached */
}
