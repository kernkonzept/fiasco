/* -*- c -*- */

.section .text.init,"ax"
.type _start_kernel,@function
.globl  _start_kernel
_start_kernel:

.option push
.option norelax
  la gp, __global_pointer$
.option pop

  la sp, _stack
  mv a0, tp
  call __main;

 /* never returns */

.bss
.balign 16
.globl _sstack
_sstack:
  .space  4096
.globl _stack
_stack:
