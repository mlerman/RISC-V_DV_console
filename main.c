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
void uart_puts(const char *s);
void uart_putc(char c);
void uart_init();
void aplic_m_init();
char uart_getc();
void do_smode();
void do_mmode();
void do_whoami();
void do_whoami_hw();
void do_harts();
void do_clint();
void detect_num_harts();
void do_dbg();
void do_aplic();
void do_pmp();
void do_ipitest();
void do_nexth();
void do_layout();
void halt();

#define MODE_M 0
#define MODE_S 1
extern volatile int current_mode;
extern volatile int ecall_pending;

#define MAX_CMD 32

void read_line(char *buf) {
    int i = 0;
    char c;
    while (1) {
        c = uart_getc();
        if (c == '\r' || c == '\n') {
            buf[i] = '\0';
            uart_puts("\r\n");
            return;
        }
        if (i < MAX_CMD - 1) {
            buf[i++] = c;
            uart_putc(c);
        }
    }
}

int str_eq(const char *a, const char *b) {
    while (*a && *b) if (*a++ != *b++) return 0;
    return *a == *b;
}

void halt() {
    volatile int *test = (volatile int *)0x100000;
    *test = 0x5555;
}

/* ── linker-exported section boundaries ──────────────────────────────── */
extern char __text_start,  __text_end;
extern char __rodata_start, __rodata_end;
extern char __data_start,  __data_end;
extern char __bss_start,   __bss_end;
extern char __stack_bottom;
extern char stack_top;
extern char __stack1_bottom;
extern char stack1_top;
extern char __stack2_bottom;
extern char stack2_top;
extern char __stack3_bottom;
extern char stack3_top;
extern char __stack4_bottom;
extern char stack4_top;
extern char __stack5_bottom;
extern char stack5_top;
extern char __stack6_bottom;
extern char stack6_top;
extern char __stack7_bottom;
extern char stack7_top;
extern char __stack8_bottom;
extern char stack8_top;
extern char __stack9_bottom;
extern char stack9_top;
extern char __stack10_bottom;
extern char stack10_top;
extern char __stack11_bottom;
extern char stack11_top;
extern char __stack12_bottom;
extern char stack12_top;
extern char __stack13_bottom;
extern char stack13_top;
extern char __stack14_bottom;
extern char stack14_top;
extern char __stack15_bottom;
extern char stack15_top;

/* ── layout helpers ───────────────────────────────────────────────────── */

static void print_addr(unsigned long addr) {
    const char hex[] = "0123456789abcdef";
    char buf[11];
    buf[0]='0'; buf[1]='x';
    for (int i = 0; i < 8; i++)
        buf[2+i] = hex[(addr >> (28 - i*4)) & 0xf];
    buf[10] = '\0';
    uart_puts(buf);
}

static void print_size(unsigned long sz) {
    unsigned long val;
    const char *unit;
    if (sz >= 1024*1024)      { val = sz/(1024*1024); unit = " MB"; }
    else if (sz >= 1024)       { val = sz/1024;        unit = " KB"; }
    else                       { val = sz;              unit = " B";  }
    char tmp[12]; int t = 0;
    if (val == 0) { tmp[t++] = '0'; }
    else { unsigned long v = val; while(v){ tmp[t++]='0'+(v%10); v/=10; } }
    /* reverse */
    for (int i = 0, j = t-1; i < j; i++, j--) {
        char c = tmp[i]; tmp[i] = tmp[j]; tmp[j] = c;
    }
    tmp[t] = '\0';
    uart_puts(tmp);
    uart_puts(unit);
}

#define BAR_WIDTH 12

static void print_region(unsigned long addr, const char *label, unsigned long size) {
    print_addr(addr);
    uart_puts(" +");
    for (int i = 0; i < BAR_WIDTH; i++) uart_putc('#');
    uart_puts("+ ");
    uart_puts(label);
    if (size) { uart_puts(" ("); print_size(size); uart_puts(")"); }
    uart_puts("\r\n");
    uart_puts("           |");
    for (int i = 0; i < BAR_WIDTH; i++) uart_putc('#');
    uart_puts("|\r\n");
}

static void print_border(unsigned long addr, const char *label) {
    print_addr(addr);
    uart_puts(" +");
    for (int i = 0; i < BAR_WIDTH; i++) uart_putc('-');
    uart_puts("+ ");
    uart_puts(label);
    uart_puts("\r\n");
}

static void print_gap(const char *label) {
    uart_puts("           |");
    uart_puts("  ~ ~ ~ ~  ");
    uart_puts(" |  (");
    uart_puts(label);
    uart_puts(")\r\n");
}

static void print_device(unsigned long addr, const char *label) {
    print_addr(addr);
    uart_puts("  -- ");
    uart_puts(label);
    uart_puts("\r\n");
}

void do_layout() {
    unsigned long text_start  = (unsigned long)&__text_start;
    unsigned long text_end    = (unsigned long)&__text_end;
    unsigned long rodata_start= (unsigned long)&__rodata_start;
    unsigned long rodata_end  = (unsigned long)&__rodata_end;
    unsigned long data_start  = (unsigned long)&__data_start;
    unsigned long data_end    = (unsigned long)&__data_end;
    unsigned long bss_start   = (unsigned long)&__bss_start;
    unsigned long bss_end     = (unsigned long)&__bss_end;
    unsigned long stk_bottom  = (unsigned long)&__stack_bottom;
    unsigned long stk_top     = (unsigned long)&stack_top;
    unsigned long stk1_bottom = (unsigned long)&__stack1_bottom;
    unsigned long stk1_top    = (unsigned long)&stack1_top;
    unsigned long stk2_bottom = (unsigned long)&__stack2_bottom;
    unsigned long stk2_top    = (unsigned long)&stack2_top;
    unsigned long stk3_bottom = (unsigned long)&__stack3_bottom;
    unsigned long stk3_top    = (unsigned long)&stack3_top;
    unsigned long stk4_bottom = (unsigned long)&__stack4_bottom;
    unsigned long stk4_top    = (unsigned long)&stack4_top;
    unsigned long stk5_bottom = (unsigned long)&__stack5_bottom;
    unsigned long stk5_top    = (unsigned long)&stack5_top;
    unsigned long stk6_bottom = (unsigned long)&__stack6_bottom;
    unsigned long stk6_top    = (unsigned long)&stack6_top;
    unsigned long stk7_bottom = (unsigned long)&__stack7_bottom;
    unsigned long stk7_top    = (unsigned long)&stack7_top;
    unsigned long stk8_bottom = (unsigned long)&__stack8_bottom;
    unsigned long stk8_top    = (unsigned long)&stack8_top;
    unsigned long stk9_bottom = (unsigned long)&__stack9_bottom;
    unsigned long stk9_top    = (unsigned long)&stack9_top;
    unsigned long stk10_bottom = (unsigned long)&__stack10_bottom;
    unsigned long stk10_top    = (unsigned long)&stack10_top;
    unsigned long stk11_bottom = (unsigned long)&__stack11_bottom;
    unsigned long stk11_top    = (unsigned long)&stack11_top;
    unsigned long stk12_bottom = (unsigned long)&__stack12_bottom;
    unsigned long stk12_top    = (unsigned long)&stack12_top;
    unsigned long stk13_bottom = (unsigned long)&__stack13_bottom;
    unsigned long stk13_top    = (unsigned long)&stack13_top;
    unsigned long stk14_bottom = (unsigned long)&__stack14_bottom;
    unsigned long stk14_top    = (unsigned long)&stack14_top;
    unsigned long stk15_bottom = (unsigned long)&__stack15_bottom;
    unsigned long stk15_top    = (unsigned long)&stack15_top;
    unsigned long ram_top     = 0x80000000UL + (128UL * 1024 * 1024);

    uart_puts("\r\nMemory Layout\r\n");
    uart_puts("=============\r\n");

    /* ── RAM region (top → bottom) ── */
    print_border(ram_top,    "RAM top");
    print_gap("free RAM");
    if (num_harts > 15) print_region(stk15_bottom, ".stack (hart 15)", stk15_top - stk15_bottom);
    if (num_harts > 14) print_region(stk14_bottom, ".stack (hart 14)", stk14_top - stk14_bottom);
    if (num_harts > 13) print_region(stk13_bottom, ".stack (hart 13)", stk13_top - stk13_bottom);
    if (num_harts > 12) print_region(stk12_bottom, ".stack (hart 12)", stk12_top - stk12_bottom);
    if (num_harts > 11) print_region(stk11_bottom, ".stack (hart 11)", stk11_top - stk11_bottom);
    if (num_harts > 10) print_region(stk10_bottom, ".stack (hart 10)", stk10_top - stk10_bottom);
    if (num_harts > 9) print_region(stk9_bottom, ".stack (hart 9)", stk9_top - stk9_bottom);
    if (num_harts > 8) print_region(stk8_bottom, ".stack (hart 8)", stk8_top - stk8_bottom);
    if (num_harts > 7) print_region(stk7_bottom, ".stack (hart 7)", stk7_top - stk7_bottom);
    if (num_harts > 6) print_region(stk6_bottom, ".stack (hart 6)", stk6_top - stk6_bottom);
    if (num_harts > 5) print_region(stk5_bottom, ".stack (hart 5)", stk5_top - stk5_bottom);
    if (num_harts > 4) print_region(stk4_bottom, ".stack (hart 4)", stk4_top - stk4_bottom);
    if (num_harts > 3) print_region(stk3_bottom, ".stack (hart 3)", stk3_top - stk3_bottom);
    if (num_harts > 2) print_region(stk2_bottom, ".stack (hart 2)", stk2_top - stk2_bottom);
    if (num_harts > 1) print_region(stk1_bottom, ".stack (hart 1)", stk1_top - stk1_bottom);
    print_region(stk_bottom,  ".stack (hart 0)", stk_top  - stk_bottom);
    if (bss_end   > bss_start)   print_region(bss_start,   ".bss",    bss_end   - bss_start);
    if (data_end  > data_start)  print_region(data_start,  ".data",   data_end  - data_start);
    if (rodata_end> rodata_start)print_region(rodata_start,".rodata", rodata_end- rodata_start);
    print_region(text_start, ".text",    text_end - text_start);
    print_border(0x80000000UL,   "RAM base (_start)");

    /* ── gap ── */
    print_gap("unmapped");

    /* ── MMIO devices ── */
    uart_puts("\r\nMMIO Devices\r\n");
    print_device(0x10000000UL, "UART0        (NS16550)");
    print_device(0x0d000000UL, "S-APLIC");
    print_device(0x0c000000UL, "M-APLIC");
    print_device(0x02000000UL, "CLINT");
    print_device(0x00100000UL, "TEST / RESET");
    print_device(0x00001000UL, "BOOT ROM");
    print_device(0x00000000UL, "-- addr 0 --");
    uart_puts("=============\r\n");
}


/*
 * clint? — read-only dump of CLINT registers for all harts.
 *
 * CLINT layout (confirmed from device tree, -smp 2):
 *   msip:      base + 4*hart          (1 word per hart)
 *   mtimecmp:  base + 0x4000 + 8*hart (1 dword per hart)
 *   mtime:     base + 0xbff8          (single shared dword)
 *
 * Only callable from M-mode (reads MMIO directly).
 * Safe: read-only, no side effects.
 */
#define CLINT_BASE     0x02000000UL
#define CLINT_MSIP(h)     (CLINT_BASE + 0x0000 + 4*(h))
#define CLINT_MTIMECMP(h) (CLINT_BASE + 0x4000 + 8*(h))
#define CLINT_MTIME       (CLINT_BASE + 0xbff8)
#define CLINT_NUM_HARTS   NUM_HARTS

/* print a full 64-bit value as 16 hex digits */
static void print_addr64(unsigned long val) {
    const char hex[] = "0123456789abcdef";
    char buf[19];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 16; i++)
        buf[2+i] = hex[(val >> (60 - i*4)) & 0xf];
    buf[18] = '\0';
    uart_puts(buf);
}

void do_clint() {
    if (current_mode != MODE_M) {
        uart_puts("clint? is only available in M-mode\r\n");
        return;
    }

    volatile unsigned long *mtime_reg =
        (volatile unsigned long *)CLINT_MTIME;

    /* sample mtime twice to show it's running */
    unsigned long t1 = *mtime_reg;
    /* small busy delay */
    for (volatile int i = 0; i < 1000; i++);
    unsigned long t2 = *mtime_reg;

    uart_puts("\r\nCLINT Register Dump\r\n");
    uart_puts("===================\r\n");
    uart_puts("mtime:      ");
    print_addr64(t2);
    uart_puts("\r\n");
    uart_puts("mtime delta (~1k cycles): +");
    print_size(t2 - t1);
    uart_puts("\r\n\r\n");

    uart_puts("Hart  msip  timer-pending  mtimecmp\r\n");
    uart_puts("----  ----  -------------  --------\r\n");

    for (int h = 0; h < (int)num_harts; h++) {
        volatile unsigned int  *msip     =
            (volatile unsigned int  *)CLINT_MSIP(h);
        volatile unsigned long *mtimecmp =
            (volatile unsigned long *)CLINT_MTIMECMP(h);

        unsigned long cmp = *mtimecmp;
        int timer_pending = (t2 >= cmp);

        /* hart id */
        uart_puts("  ");
        uart_putc('0' + h);
        uart_puts("     ");

        /* msip */
        uart_putc('0' + (*msip & 1));
        uart_puts("     ");

        /* timer pending */
        uart_puts(timer_pending ? "YES            " : "no             ");

        /* mtimecmp — full 64-bit */
        print_addr64(cmp);
        uart_puts("\r\n");
    }
    uart_puts("===================\r\n");
}


/*
 * ipitest! — Stage 3 IPI echo test.
 *
 * Sends an IPI to hart 1 by writing 1 to its CLINT msip register.
 * Hart 1's trap handler clears msip and sets ipi_ack = 1.
 * We busy-wait up to ~100M cycles for the ack, then report.
 *
 * Writes: CLINT msip[1]  → cmd! by convention.
 * Only available from M-mode.
 */
#define CLINT_MSIP1 (*((volatile unsigned int *)(0x02000000UL + 4*1)))
#define IPI_TIMEOUT 100000000UL

extern volatile int ipi_ack;

void do_ipitest() {
    if (current_mode != MODE_M) {
        uart_puts("ipitest! is only available in M-mode\r\n");
        return;
    }

    uart_puts("Sending IPI to hart 1... ");

    ipi_ack = 0;

    /* fire the IPI */
    CLINT_MSIP1 = 1;

    /* busy-wait for ack */
    unsigned long timeout = IPI_TIMEOUT;
    while (!ipi_ack && timeout--);

    if (ipi_ack)
        uart_puts("ACK received. IPI path OK\r\n");
    else
        uart_puts("TIMEOUT. No ACK from hart 1\r\n");
}

/*
 * detect_num_harts — count harts by probing M-APLIC IDC registers.
 *
 * Each hart has an IDC block in M-APLIC at:
 *   0x0c004000 + hart * 0x20
 *
 * The idelivery register at offset +0x00 is 1-bit wide.
 * Bits 31:1 are RESERVED and must read back as 0 per RISC-V spec.
 *
 * Method: write 0xFFFFFFFE (all reserved bits set, bit 0 clear),
 * then read back. A real IDC returns 0x00000000 (reserved masked).
 * A non-existent/open-bus address returns the written value or 0xFFFFFFFF.
 *
 * No faults, no timing — purely register layout signature detection.
 * Works on any AIA-compliant hardware.
 */
#define APLIC_MBASE       0x0c000000UL
#define APLIC_IDC_BASE    0x4000UL
#define APLIC_IDC_STRIDE  0x20UL

void detect_num_harts() {
    /*
     * Wait for secondary harts to check in via hart_present[].
     *
     * Strategy: poll until the count has been stable for 1M cycles.
     * Reset the stability counter every time a new hart checks in.
     * This naturally adapts to slow-starting harts without a fixed wait.
     * Hard timeout at 2B cycles prevents infinite loop if harts never arrive.
     */
    unsigned long last_count = 0;
    long stable = 0;

    for (volatile long i = 0; i < 2000000000L; i++) {
        unsigned long count = 0;
        for (int h = 1; h < HART_PRESENT_MAX; h++)
            if (hart_present[h]) count++;

        if (count == last_count) {
            /* stable — check if long enough */
            if (++stable >= 1000000) break;
        } else {
            /* new hart checked in — reset stability counter */
            last_count = count;
            stable = 0;
        }
    }

    /* count all present harts including hart 0 */
    unsigned long count = 1;
    for (int h = 1; h < HART_PRESENT_MAX; h++) {
        if (hart_present[h])
            count++;
        else
            break;
    }
    num_harts = count;
}


/*
 * aplic? — dump APLIC configuration for current hart and privilege mode.
 *
 * Shows domain-level config, UART source config, and the IDC block
 * for the current hart. Useful for understanding APLIC state and
 * finding reliable signatures for hart detection.
 *
 * M-APLIC base: 0x0c000000
 * S-APLIC base: 0x0d000000
 * IDC stride:   0x20 per hart
 */
#define DUMP_APLIC_BASE  0x0c000000UL
#define DUMP_SBASE       0x0d000000UL
#define DUMP_DOMAINCFG   0x0000UL
#define DUMP_SOURCECFG   0x0004UL  /* + (irq-1)*4 */
#define DUMP_SETIP_BASE  0x1c00UL  /* pending array */
#define DUMP_SETIE_BASE  0x1e00UL  /* enable array */
#define DUMP_IDC_BASE    0x4000UL
#define DUMP_IDC_STRIDE  0x20UL
#define DUMP_UART_IRQ    10

static void print_reg32(const char *label, unsigned long addr) {
    const char hx[] = "0123456789abcdef";
    unsigned int val = *(volatile unsigned int *)addr;
    uart_puts("  ");
    uart_puts(label);
    uart_puts(": 0x");
    for (int i = 28; i >= 0; i -= 4)
        uart_putc(hx[(val >> i) & 0xf]);
    uart_puts("\r\n");
}

static void dump_aplic_idc(unsigned long base, unsigned long hart) {
    unsigned long idc = base + DUMP_IDC_BASE + hart * DUMP_IDC_STRIDE;
    uart_puts("  IDC[hart ");
    uart_putc('0' + (char)hart);
    uart_puts("]:\r\n");
    print_reg32("    idelivery ", idc + 0x00);
    print_reg32("    ithreshold", idc + 0x08);
    print_reg32("    topi      ", idc + 0x18);
    print_reg32("    claimi    ", idc + 0x1c);
}

static void dump_aplic_domain(unsigned long base, const char *name) {
    uart_puts(name);
    uart_puts(" domain:\r\n");
    print_reg32("domaincfg    ", base + DUMP_DOMAINCFG);
    print_reg32("sourcecfg[10]", base + DUMP_SOURCECFG + (DUMP_UART_IRQ-1)*4);
    print_reg32("setip[0]     ", base + DUMP_SETIP_BASE);
    print_reg32("setie[0]     ", base + DUMP_SETIE_BASE);

    /* IDC for current hart */
    dump_aplic_idc(base, console_hartid);
}

void do_aplic() {
    uart_puts("\r\nAPLIC Register Dump\r\n");
    uart_puts("===================\r\n");
    if (current_mode == MODE_M)
        dump_aplic_domain(DUMP_APLIC_BASE, "M-APLIC");
    else
        dump_aplic_domain(DUMP_SBASE, "S-APLIC");
    uart_puts("===================\r\n");
}


/*
 * pmp? — dump PMP configuration for the current hart.
 *
 * PMP registers are per-hart CSRs. Hart 0 configures them in _start,
 * secondary harts in hang_secondary. Shows pmpcfg0 and pmpaddr0.
 *
 * pmpcfg0 layout (per entry, 8 bits):
 *   bits 7:   L (lock)
 *   bits 6:5  reserved (must be 0)
 *   bits 4:3  A (address matching: 0=off,1=TOR,2=NA4,3=NAPOT)
 *   bit  2:   X (execute)
 *   bit  1:   W (write)
 *   bit  0:   R (read)
 */
void do_pmp() {
    const char hx[] = "0123456789abcdef";

    unsigned long pmpcfg0, pmpaddr0;
    __asm__ volatile ("csrr %0, pmpcfg0"  : "=r"(pmpcfg0));
    __asm__ volatile ("csrr %0, pmpaddr0" : "=r"(pmpaddr0));

    uart_puts("\r\nPMP Register Dump (hart ");
    uart_putc('0' + (char)console_hartid);
    uart_puts(")\r\n");
    uart_puts("===================\r\n");

    /* pmpcfg0 */
    uart_puts("pmpcfg0 : 0x");
    for (int i = 60; i >= 0; i -= 4)
        uart_putc(hx[(pmpcfg0 >> i) & 0xf]);
    uart_puts("\r\n");

    /* decode entry 0 (bits 7:0 of pmpcfg0) */
    unsigned int cfg = pmpcfg0 & 0xff;
    uart_puts("  entry0: ");
    uart_puts((cfg & (1<<7)) ? "L " : ". ");  /* lock */
    unsigned int A = (cfg >> 3) & 0x3;
    if      (A == 0) uart_puts("OFF   ");
    else if (A == 1) uart_puts("TOR   ");
    else if (A == 2) uart_puts("NA4   ");
    else             uart_puts("NAPOT ");
    uart_puts((cfg & (1<<2)) ? "X" : ".");
    uart_puts((cfg & (1<<1)) ? "W" : ".");
    uart_puts((cfg & (1<<0)) ? "R" : ".");
    uart_puts("\r\n");

    /* pmpaddr0 — NAPOT encoded, actual range = 2^(trailing_ones+3) bytes */
    uart_puts("pmpaddr0: 0x");
    for (int i = 60; i >= 0; i -= 4)
        uart_putc(hx[(pmpaddr0 >> i) & 0xf]);
    uart_puts("\r\n");

    /* decode NAPOT range */
    if (A == 3) {
        /* count trailing ones to get size */
        unsigned long size = 8;  /* minimum 8 bytes */
        unsigned long tmp = pmpaddr0;
        while (tmp & 1) { size <<= 1; tmp >>= 1; }
        uart_puts("  NAPOT range: ");
        print_size(size);
        uart_puts("\r\n");
    }

    uart_puts("===================\r\n");
}


/*
 * dbg — show internal firmware state for debugging.
 * Shows SMP shared variables, hart arrays, and key flags.
 */
void do_dbg() {
    extern volatile int ecall_pending;
    extern volatile int hw_probe_fault;
    extern volatile int ipi_ack;
    const char hx[] = "0123456789abcdef";

    uart_puts("\r\nDebug Info\r\n");
    uart_puts("==========\r\n");

    /* num_harts */
    uart_puts("num_harts:      ");
    uart_putc('0' + (char)num_harts);
    uart_puts("\r\n");

    /* hart_present */
    uart_puts("hart_present:   [");
    for (int h = 0; h < (int)num_harts; h++) {
        uart_putc('0' + h);
        uart_puts("=");
        uart_putc('0' + (char)(hart_present[h] & 1));
        if (h < (int)num_harts - 1) uart_puts(" ");
    }
    uart_puts("]\r\n");

    /* hart_state */
    uart_puts("hart_state:     [");
    for (int h = 0; h < (int)num_harts; h++) {
        uart_putc('0' + h);
        uart_puts("=");
        uart_puts(hart_state[h] == HART_ACTIVE ? "ACT" : "PRK");
        if (h < (int)num_harts - 1) uart_puts(" ");
    }
    uart_puts("]\r\n");

    /* console_hartid */
    uart_puts("console_hartid: ");
    uart_putc('0' + (char)console_hartid);
    uart_puts("\r\n");

    /* hart_entry */
    uart_puts("hart_entry:     0x");
    unsigned long he = (unsigned long)hart_entry;
    for (int i = 60; i >= 0; i -= 4)
        uart_putc(hx[(he >> i) & 0xf]);
    uart_puts("\r\n");

    /* flags */
    uart_puts("ecall_pending:  ");
    uart_putc('0' + (char)(ecall_pending & 1));
    uart_puts("\r\n");

    uart_puts("hw_probe_fault: ");
    uart_putc('0' + (char)(hw_probe_fault & 1));
    uart_puts("\r\n");

    uart_puts("ipi_ack:        ");
    uart_putc('0' + (char)(ipi_ack & 1));
    uart_puts("\r\n");

    uart_puts("==========\r\n");
}

void do_help() {
    uart_puts("Commands:\r\n");
    uart_puts("  help       — list commands\r\n");
  uart_puts("  dbg        — show internal firmware state\r\n");
    uart_puts("  layout     — show memory map\r\n");
    uart_puts("  harts?     — show info about active CPU cores\r\n");
  uart_puts("  clint?     — dump CLINT msip/mtimecmp/mtime registers\r\n");
  uart_puts("  aplic?     — dump APLIC config for current hart/mode\r\n");
  uart_puts("  pmp?       — dump PMP config for current hart\r\n");
  uart_puts("  ipitest!   — send IPI to hart 1 and wait for ack\r\n");
  uart_puts("  nexth!     — hand console to the other hart\r\n");
    uart_puts("  whoami     — show current privilege mode (software variable)\r\n");
    uart_puts("  whoami?    — probe privilege mode from hardware registers\r\n");
    uart_puts("  smode!     — switch to S-mode\r\n");
    uart_puts("  mmode!     — switch to M-mode\r\n");
    uart_puts("  reset!     — full system reset\r\n");
    uart_puts("  exit       — halt\r\n");
    uart_puts("Convention:\r\n");
    uart_puts("  cmd!  writes hardware registers\r\n");
    uart_puts("  cmd?  reads hardware registers\r\n");
    uart_puts("  cmd   purely informative\r\n");
}

void dispatch(char *cmd) {
    if (str_eq(cmd, "help"))
        do_help();
    else if (str_eq(cmd, "dbg"))
        do_dbg();
    else if (str_eq(cmd, "harts?"))
        do_harts();
    else if (str_eq(cmd, "clint?"))
        do_clint();
    else if (str_eq(cmd, "aplic?"))
        do_aplic();
    else if (str_eq(cmd, "pmp?"))
        do_pmp();
    else if (str_eq(cmd, "ipitest!"))
        do_ipitest();
    else if (str_eq(cmd, "nexth!"))
        do_nexth();
    else if (str_eq(cmd, "whoami"))
        do_whoami();
    else if (str_eq(cmd, "whoami?"))
        do_whoami_hw();
    else if (str_eq(cmd, "layout"))
        do_layout();
    else if (str_eq(cmd, "smode!")) {
        if (current_mode == MODE_S)
            uart_puts("Already in S-mode\r\n");
        else
            do_smode();
    } else if (str_eq(cmd, "mmode!")) {
        if (current_mode == MODE_M)
            uart_puts("Already in M-mode\r\n");
        else {
            /* trigger ecall to return to M-mode */
            uart_puts("Requesting M-mode via ecall...\r\n");
            __asm__ volatile ("ecall");
            /* returns here after ecall handled */
        }
    } else if (str_eq(cmd, "reset!")) {
        uart_puts("Performing full system reset...\r\n");
        /* Trigger QEMU full system reset */
        volatile int *test = (volatile int *)0x100000;
        *test = 0x7777;
        while (1); /* Should not be reached */
    } else if (str_eq(cmd, "exit")) {
        uart_puts("Goodbye!\r\n");
        halt();
    } else {
        uart_puts("Unknown command. Type 'help'\r\n");
    }
}

void prompt() {
    uart_puts("[");
    uart_putc('0' + (char)console_hartid);
    uart_puts("|");
    if (current_mode == MODE_M)
        uart_puts("M] > ");
    else
        uart_puts("S] > ");
}

/* M-mode main loop */
void m_mode_loop() {
    char cmd[MAX_CMD];
    while (1) {
        prompt();
        read_line(cmd);
        dispatch(cmd);
    }
}

/* S-mode main loop */
void s_mode_loop() {
    char cmd[MAX_CMD];
    while (1) {
        /* check if ecall was triggered (mmode! request) */
        if (ecall_pending) {
            ecall_pending = 0;
            do_mmode();
            /* never returns */
        }
        prompt();
        read_line(cmd);
        dispatch(cmd);
    }
}

/* entry point called from start.S */
void main() {
    uart_init();
    detect_num_harts();   /* probe before APLIC init — no state to disrupt */
    aplic_m_init();
    uart_puts("RISC-V DV Console by Mikhael Lerman https://checkthisresume.com\r\n");
    uart_puts("Type 'help' for commands\r\n");
    console_hartid = 0;  /* hart 0 starts as console owner */
    m_mode_loop();
}