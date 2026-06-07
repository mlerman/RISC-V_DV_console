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
#define UART_IRQ 10

char uart_getc_raw();
int aplic_m_claim_irq();
int aplic_s_claim_irq();
void aplic_complete_irq(int irq);
void uart_puts(const char *s);
void uart_putc(char c);

/* shared RX ring buffer */
#define RX_BUF_SIZE 64
static volatile char rx_buf[RX_BUF_SIZE];
static volatile int rx_head = 0;
static volatile int rx_tail = 0;

void rx_buf_push(char c) {
    int next = (rx_head + 1) % RX_BUF_SIZE;
    if (next != rx_tail) {
        rx_buf[rx_head] = c;
        rx_head = next;
    }
}

int rx_buf_empty() {
    return rx_head == rx_tail;
}

char rx_buf_pop() {
    char c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
    return c;
}

char uart_getc() {
    while (rx_buf_empty());
    return rx_buf_pop();
}

/* Set by m_trap_handler on illegal instruction; cleared by probe caller */
volatile int hw_probe_fault = 0;

/* M-mode trap handler */
void m_trap_handler() {
    unsigned long mcause;
    __asm__ volatile ("csrr %0, mcause" : "=r"(mcause));

    if (mcause & (1UL << 63)) {
        /* interrupt */
        unsigned long cause = mcause & 0x7fffffffffffffff;
        if (cause == 11) {
            /* M-mode external interrupt via M-APLIC */
            int irq = aplic_m_claim_irq();
            if (irq == UART_IRQ) {
                char c = uart_getc_raw();
                rx_buf_push(c);
            }
            aplic_complete_irq(irq);
        }
    } else {
        /* exception */
        unsigned long cause = mcause & 0x7fffffffffffffff;
        if (cause == 2) {
            /* Illegal instruction — caused by probing an M-only CSR from S-mode */
            unsigned long mepc;
            __asm__ volatile ("csrr %0, mepc" : "=r"(mepc));
            mepc += 4;
            __asm__ volatile ("csrw mepc, %0" : : "r"(mepc));
            hw_probe_fault = 1;
        } else if (cause == 9) {
            /* S-mode ecall — mmode! request */
            unsigned long mepc;
            __asm__ volatile ("csrr %0, mepc" : "=r"(mepc));
            mepc += 4;
            __asm__ volatile ("csrw mepc, %0" : : "r"(mepc));

            /* Force privilege transition to M-mode upon mret */
            unsigned long mstatus;
            __asm__ volatile ("csrr %0, mstatus" : "=r"(mstatus));
            mstatus &= ~(3UL << 11); // Clear current MPP
            mstatus |= (3UL << 11);  // Set MPP to 3 (Machine Mode)
            __asm__ volatile ("csrw mstatus, %0" : : "r"(mstatus));

            /* signal mode switch — handled in main loop */
            extern volatile int ecall_pending;
            ecall_pending = 1;
        }
    }
}


/* S-mode trap handler */
void s_trap_handler() {
    unsigned long scause;
    __asm__ volatile ("csrr %0, scause" : "=r"(scause));

    if (scause & (1UL << 63)) {
        unsigned long cause = scause & 0x7fffffffffffffff;
        if (cause == 9) {
            /* S-mode external interrupt via S-APLIC */
            int irq = aplic_s_claim_irq();
            if (irq == UART_IRQ) {
                char c = uart_getc_raw();
                rx_buf_push(c);
            }
            aplic_complete_irq(irq);
        }
    }
}
/* appended SMP IPI handlers — replaces the echo-only version */

volatile int ipi_ack = 0;

#define CLINT_BASE    0x02000000UL
#define CLINT_MSIP(h) (*((volatile unsigned int *)(CLINT_BASE + 4*(h))))

void m_mode_loop();
void aplic_retarget_idc(int hart);

#include "smp.h"

static void setup_full_mmode_traps() {
    unsigned long mtvec_val;
    /* install full M-mode trap handler */
    __asm__ volatile (
        ".option push;"
        ".option norelax;"
        "la %0, m_trap_entry;"
        ".option pop;"
        : "=r"(mtvec_val)
    );
    __asm__ volatile ("csrw mtvec, %0" :: "r"(mtvec_val));

    /* mie: enable external interrupts (MEIE bit 11) */
    __asm__ volatile ("li t0, (1 << 11); csrw mie, t0" ::: "t0");

    /*
     * mstatus: MPP=M (bits 12:11 = 11), MPIE=1 (bit 7)
     * Do NOT set MIE here — mret will copy MPIE->MIE automatically.
     * Jumping directly from a trap handler means we must use mret
     * to properly restore interrupt state.
     */
    unsigned long mstatus_val = (3UL << 11) | (1UL << 7);
    __asm__ volatile ("csrw mstatus, %0" :: "r"(mstatus_val));
}

/* setup_park_traps removed — logic now lives in do_nexth() */

/*
 * secondary_trap_handler — generic park handler for harts 1, 2, 3.
 * Reads mhartid to identify itself — works for any secondary hart.
 *
 * mcause=3 (M software interrupt):
 *   hart_entry == 0  → ipitest! echo, just ack
 *   hart_entry != 0  → nexth! handoff, take over console via mret
 */
void secondary_trap_handler() {
    unsigned long mcause, my_hart;
    __asm__ volatile ("csrr %0, mcause" : "=r"(mcause));
    __asm__ volatile ("csrr %0, mhartid" : "=r"(my_hart));

    if (mcause & (1UL << 63)) {
        unsigned long cause = mcause & 0x7fffffffffffffff;
        if (cause == 3) {
            /* clear our own msip */
            CLINT_MSIP(my_hart) = 0;

            if (hart_entry == 0) {
                /* ipitest! echo — just ack */
                ipi_ack = 1;
                return;
            }

            /* nexth! — take over console */
            hart_state[my_hart] = HART_ACTIVE;
            setup_full_mmode_traps();

            unsigned long entry_addr = (unsigned long)hart_entry;
            hart_entry = 0;
            __asm__ volatile ("csrw mepc, %0" :: "r"(entry_addr));
            return;
        }
    }
}

void hart0_trap_handler() {
    unsigned long mcause;
    __asm__ volatile ("csrr %0, mcause" : "=r"(mcause));

    if (mcause & (1UL << 63)) {
        unsigned long cause = mcause & 0x7fffffffffffffff;

        if (cause == 3) {
            /* M-mode software interrupt — IPI from hartswitch! */
            CLINT_MSIP(0) = 0;

            if (hart_entry == 0)
                return;  /* spurious */

            /* hartswitch! back to hart 0 */
            hart_state[0] = HART_ACTIVE;
            setup_full_mmode_traps();

            unsigned long entry_addr = (unsigned long)hart_entry;
            hart_entry = 0;
            __asm__ volatile ("csrw mepc, %0" :: "r"(entry_addr));
            return;

        } else if (cause == 11) {
            /* M-mode external interrupt — UART RX while parked.
             * Hart 0 keeps handling UART even while hart 1 owns
             * the console. Bytes go into the shared rx_buf which
             * hart 1 reads via uart_getc(). */
            int irq = aplic_m_claim_irq();
            if (irq == UART_IRQ) {
                char c = uart_getc_raw();
                rx_buf_push(c);
            }
            aplic_complete_irq(irq);
        }
    }
}
