/* -*- c -*- */
#include <asm_riscv.h>

.section .text.init,"ax"
.type start,@function
.globl _start
_start:
  la t1, _bootstrap_hart_lottery
  li t0, 1
  amoadd.w t1, t0, (t1)

  /* first hart progresses */
  bnez t1, _wait_for_bootstrap

  la sp, _bstrap_stack
  call bootstrap_main;
  /* never returns */

_wait_for_bootstrap:
  REG_L t0, (_secondary_wait_for_bootstrap_satp)
  REG_L t1, (_secondary_wait_for_bootstrap_entry)
  beqz t0, _wait_for_bootstrap
  beqz t1, _wait_for_bootstrap

  /* enable paging */
  sfence.vma
  csrw satp, t0

  /* enter kernel */
  jr t1

.globl _start_hsm
_start_hsm:
  /* store hartid in tp register (just as the bootstrap boot loader) */
  mv tp, a0
  j _start

.bss
.balign 8
.globl _secondary_wait_for_bootstrap_satp
_secondary_wait_for_bootstrap_satp:
  .8byte 0

.globl _secondary_wait_for_bootstrap_entry
_secondary_wait_for_bootstrap_entry:
  .8byte 0

_bootstrap_hart_lottery:
  .4byte 0

.balign 16
  .space 2048
.globl _bstrap_stack
_bstrap_stack:
