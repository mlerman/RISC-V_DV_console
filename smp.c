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
/*
 * smp.c — shared multi-hart state
 *
 * Single source of truth for hart count and runtime state.
 * All other files reference these via smp.h extern declarations.
 */

#include "smp.h"

/*
 * hart_state[] — tracks what each hart is currently doing.
 * Updated by start.S on boot, and by hartswitch! on transition.
 */
volatile hart_state_t hart_state[NUM_HARTS] = {
    [0] = HART_PARKED,   /* overwritten to ACTIVE by start.S before main() */
    [1] = HART_PARKED,
    [2] = HART_PARKED,
    [3] = HART_PARKED,
    [4] = HART_PARKED,
    [5] = HART_PARKED,
    [6] = HART_PARKED,
    [7] = HART_PARKED,
    [8] = HART_PARKED,
    [9] = HART_PARKED,
    [10] = HART_PARKED,
    [11] = HART_PARKED,
    [12] = HART_PARKED,
    [13] = HART_PARKED,
    [14] = HART_PARKED,
    [15] = HART_PARKED,
};

/*
 * hart_entry — rendezvous function pointer.
 * Sending hart writes this before firing IPI.
 * Waking hart reads and jumps to it from trap handler.
 */
volatile void (*hart_entry)(void) = 0;

volatile unsigned long console_hartid = 0;

/* Actual hart count — detected at boot, replaces NUM_HARTS at runtime */
volatile unsigned long num_harts = 1;

volatile unsigned int hart_present[HART_PRESENT_MAX] = {0};
