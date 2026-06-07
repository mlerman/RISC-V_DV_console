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
#define APLIC_MBASE       0x0c000000
#define APLIC_SBASE       0x0d000000

#define APLIC_DOMAINCFG      0x0000
#define APLIC_SOURCECFG_BASE 0x0004
#define APLIC_SETIENUM       0x1edc
#define APLIC_SETIPNUM       0x1cdc

/* Hart ID captured at M-mode init time — safe to use in S-mode claim */
static unsigned long aplic_hart_id = 0;
#define APLIC_IDC_BASE       0x4000
#define APLIC_IDC_IDELIVERY  (APLIC_IDC_BASE + 0x00)
#define APLIC_IDC_ITHRESHOLD (APLIC_IDC_BASE + 0x08)
#define APLIC_IDC_TOPI       (APLIC_IDC_BASE + 0x18)
#define APLIC_IDC_CLAIMI     (APLIC_IDC_BASE + 0x1c)
#define APLIC_IDC_STRIDE     0x20

#define UART_IRQ                10
#define SOURCECFG_SM_LEVEL_HIGH 4
#define SOURCECFG_DELEGATE      (1 << 10)
#define DOMAINCFG_IE            (1 << 8)

/* --- M-APLIC direct mode --- */
void aplic_m_init() {
    /* capture hart ID for use in claim functions */
    __asm__ volatile ("csrr %0, mhartid" : "=r"(aplic_hart_id));
    /* clear delegation - M-APLIC handles IRQ 10 directly */
    volatile unsigned int *m_sourcecfg = (volatile unsigned int *)
        (APLIC_MBASE + APLIC_SOURCECFG_BASE + (UART_IRQ - 1) * 4);
    *m_sourcecfg = SOURCECFG_SM_LEVEL_HIGH;

    volatile unsigned int *m_setienum = (volatile unsigned int *)
        (APLIC_MBASE + APLIC_SETIENUM);
    *m_setienum = UART_IRQ;

    /* use aplic_hart_id (captured above) to target correct hart's IDC */
    unsigned long m_idc_off = APLIC_IDC_BASE + aplic_hart_id * APLIC_IDC_STRIDE;

    volatile unsigned int *m_idelivery = (volatile unsigned int *)
        (unsigned long)(APLIC_MBASE + m_idc_off + 0x00);
    *m_idelivery = 1;

    volatile unsigned int *m_ithreshold = (volatile unsigned int *)
        (unsigned long)(APLIC_MBASE + m_idc_off + 0x08);
    *m_ithreshold = 0;

    volatile unsigned int *m_domaincfg = (volatile unsigned int *)
        (APLIC_MBASE + APLIC_DOMAINCFG);
    *m_domaincfg = DOMAINCFG_IE;
}

/* ---- S-APLIC direct mode --- */
void aplic_s_init() {
    /* capture hart ID — must be done here in M-mode before dropping to S */
    __asm__ volatile ("csrr %0, mhartid" : "=r"(aplic_hart_id));

    /*
     * Do NOT delegate UART IRQ to S-APLIC.
     * Hart 0 permanently owns M-APLIC UART delivery and forwards
     * bytes to shared rx_buf. S-mode reads via uart_getc().
     */
}

/* --- Claim/complete for M-APLIC --- */
/* Must read from the claiming hart's own IDC, not hart 0's */
int aplic_m_claim_irq() {
    unsigned long claimi_addr = APLIC_MBASE + APLIC_IDC_BASE
                              + aplic_hart_id * APLIC_IDC_STRIDE + 0x1c;
    volatile unsigned int *claimi = (volatile unsigned int *)claimi_addr;
    return (*claimi >> 16) & 0x3ff;
}

/* --- Claim/complete for S-APLIC --- */
int aplic_s_claim_irq() {
    /* use aplic_hart_id captured at init time — mhartid not readable in S-mode */
    unsigned long claimi_addr = APLIC_SBASE + APLIC_IDC_BASE
                              + aplic_hart_id * APLIC_IDC_STRIDE + 0x1c;
    volatile unsigned int *claimi = (volatile unsigned int *)claimi_addr;
    return (*claimi >> 16) & 0x3ff;
}

void aplic_complete_irq(int irq) {
    (void)irq;
}

/* --- Retarget UART IRQ delivery to a different hart's IDC --- */
/*
 * aplic_retarget_idc(hart) — switches UART interrupt delivery
 * to the specified hart, for both M-APLIC and S-APLIC IDC blocks.
 *
 * IDC layout (confirmed from device tree):
 *   hart N IDC base = APLIC_BASE + 0x4000 + N * 0x20
 *
 * Steps:
 *   1. Disable idelivery on all hart IDCs
 *   2. Enable idelivery only on target hart IDC
 */
void aplic_retarget_idc(int hart) {
    /* update stored hart ID so claim functions use the new owner */
    aplic_hart_id = (unsigned long)hart;

    /* disable all hart IDCs first */
    for (int h = 0; h < 2; h++) {
        volatile unsigned int *m_idel = (volatile unsigned int *)
            (unsigned long)(APLIC_MBASE + APLIC_IDC_BASE + h * APLIC_IDC_STRIDE);
        *m_idel = 0;
    }

    /* enable target hart IDC on M-APLIC */
    unsigned long m_idc_off = APLIC_IDC_BASE + (unsigned long)hart * APLIC_IDC_STRIDE;

    volatile unsigned int *m_idelivery = (volatile unsigned int *)
        (unsigned long)(APLIC_MBASE + m_idc_off + 0x00);
    *m_idelivery = 1;

    volatile unsigned int *m_ithreshold = (volatile unsigned int *)
        (unsigned long)(APLIC_MBASE + m_idc_off + 0x08);
    *m_ithreshold = 0;

    /* Re-assert source config and interrupt enable for UART IRQ
     * after IDC reconfiguration — required for QEMU APLIC to
     * re-evaluate interrupt routing to the new target hart */
    volatile unsigned int *sourcecfg = (volatile unsigned int *)
        (unsigned long)(APLIC_MBASE + APLIC_SOURCECFG_BASE + (UART_IRQ - 1) * 4);
    *sourcecfg = SOURCECFG_SM_LEVEL_HIGH;

    volatile unsigned int *setienum = (volatile unsigned int *)
        (unsigned long)(APLIC_MBASE + APLIC_SETIENUM);
    *setienum = UART_IRQ;

    volatile unsigned int *domaincfg = (volatile unsigned int *)
        (unsigned long)(APLIC_MBASE + APLIC_DOMAINCFG);
    *domaincfg = DOMAINCFG_IE;

}
