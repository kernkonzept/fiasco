/* -*- c -*- */
.section .text.init, "ax"
.global _start
_start:
    lis  %r1, __init_stack@ha
    addi %r1, %r1, __init_stack@l
    addi %r1, %r1, 16 //first stack frame
    b    bootstrap_main
1:
  b 1b
.previous
.section .bss
.align 12
.space 4096
  __init_stack:
.previous


