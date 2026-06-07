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
#ifndef SMP_H
#define SMP_H

/* Number of harts — must match -smp N in Makefile */
#define NUM_HARTS 16

typedef enum {
    HART_PARKED = 0,
    HART_ACTIVE = 1,
} hart_state_t;

extern volatile hart_state_t hart_state[NUM_HARTS];

/*
 * hart_entry — rendezvous function pointer.
 * The waking hart jumps here after receiving an IPI from hartswitch!.
 * Set by the sending hart before firing the IPI.
 */
extern volatile void (*hart_entry)(void);

/*
 * num_harts — actual hart count detected at boot by probing CLINT msip.
 * Set once by detect_num_harts() before main loop starts.
 * Use this for all runtime decisions instead of NUM_HARTS.
 * NUM_HARTS remains the compile-time maximum (stack allocation etc).
 */
extern volatile unsigned long num_harts;

/* Set to 1 by each hart during boot before parking */
#define HART_PRESENT_MAX 16
extern volatile unsigned int hart_present[HART_PRESENT_MAX];

/* Hart ID of the hart currently owning the console.
 * Set in M-mode; readable from S-mode without CSR access. */
extern volatile unsigned long console_hartid;

#endif /* SMP_H */
