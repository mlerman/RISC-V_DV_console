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
ENTRY(_start)

MEMORY {
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS {
    . = 0x80000000;

    .text : {
        __text_start = .;
        *(.text.start)
        *(.text*)
        __text_end = .;
    } > RAM

    .rodata : {
        __rodata_start = .;
        *(.rodata*)
        __rodata_end = .;
    } > RAM

    .sdata : {
        *(.sdata*)
    } > RAM

    .data : {
        __data_start = .;
        *(.data*)
        __data_end = .;
    } > RAM

    .sbss : {
        *(.sbss*)
    } > RAM

    .bss : {
        __bss_start = .;
        *(.bss*)
        __bss_end = .;
    } > RAM

    /* hart 0 stack */
    .stack : {
        . = ALIGN(16);
        __stack_bottom = .;
        . += 4096;
        stack_top = .;
    } > RAM

    /* hart 1 stack */
    .stack1 : { . = ALIGN(16); __stack1_bottom = .; . += 4096; stack1_top = .; } > RAM

    /* hart 2 stack */
    .stack2 : { . = ALIGN(16); __stack2_bottom = .; . += 4096; stack2_top = .; } > RAM

    /* hart 3 stack */
    .stack3 : { . = ALIGN(16); __stack3_bottom = .; . += 4096; stack3_top = .; } > RAM

    /* hart 4 stack */
    .stack4 : { . = ALIGN(16); __stack4_bottom = .; . += 4096; stack4_top = .; } > RAM

    /* hart 5 stack */
    .stack5 : { . = ALIGN(16); __stack5_bottom = .; . += 4096; stack5_top = .; } > RAM

    /* hart 6 stack */
    .stack6 : { . = ALIGN(16); __stack6_bottom = .; . += 4096; stack6_top = .; } > RAM

    /* hart 7 stack */
    .stack7 : { . = ALIGN(16); __stack7_bottom = .; . += 4096; stack7_top = .; } > RAM

    /* hart 8 stack */
    .stack8 : { . = ALIGN(16); __stack8_bottom = .; . += 4096; stack8_top = .; } > RAM

    /* hart 9 stack */
    .stack9 : { . = ALIGN(16); __stack9_bottom = .; . += 4096; stack9_top = .; } > RAM

    /* hart 10 stack */
    .stack10 : { . = ALIGN(16); __stack10_bottom = .; . += 4096; stack10_top = .; } > RAM

    /* hart 11 stack */
    .stack11 : { . = ALIGN(16); __stack11_bottom = .; . += 4096; stack11_top = .; } > RAM

    /* hart 12 stack */
    .stack12 : { . = ALIGN(16); __stack12_bottom = .; . += 4096; stack12_top = .; } > RAM

    /* hart 13 stack */
    .stack13 : { . = ALIGN(16); __stack13_bottom = .; . += 4096; stack13_top = .; } > RAM

    /* hart 14 stack */
    .stack14 : { . = ALIGN(16); __stack14_bottom = .; . += 4096; stack14_top = .; } > RAM

    /* hart 15 stack */
    .stack15 : { . = ALIGN(16); __stack15_bottom = .; . += 4096; stack15_top = .; } > RAM
}
