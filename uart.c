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
#define UART0 0x10000000

#define UART_THR 0
#define UART_RHR 0
#define UART_IER 1  /* interrupt enable register */
#define UART_LSR 5
#define UART_LSR_TX_IDLE  (1 << 5)
#define UART_LSR_RX_READY (1 << 0)

static volatile unsigned char *uart = (volatile unsigned char *)UART0;

void uart_putc(char c) {
    while (!(uart[UART_LSR] & UART_LSR_TX_IDLE));
    uart[UART_THR] = c;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

char uart_getc_raw() {
    return uart[UART_RHR];
}

void uart_init() {
    /* enable RX interrupt */
    uart[UART_IER] = 0x01;
}
