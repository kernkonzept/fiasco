#include <asm_riscv.h>

.global _tramp_mp_entry
_tramp_mp_entry:

  /* Wait for main hart... */
  REG_L t0, (_tramp_mp_main_hart_done)
  beqz t0, _tramp_mp_entry

  /* On entering _tramp_mp_entry, the tp register contains contains the hartid,
   * the bootstrap page table is active and we have no stack.
   */

  .option push
  .option norelax
    la gp, __global_pointer$
  .option pop

  /* Acquire spinlock */
  la t2, _tramp_mp_spinlock
  li t1, 2 /* Spin_lock::Arch_lock */
  1:
  amoor.SZMOD.aq t0, t1, (t2)
  and t0, t0, t1
  bnez t0, 1b

  /* We hold the lock, so we can use the init stack. */
  la sp, _tramp_mp_init_stack_top
  /* Pass hartid as argument */
  mv a0, tp
  tail BOOT_AP_CPU

.bss
.balign 8
.global _tramp_mp_spinlock
_tramp_mp_spinlock:
  .8byte 0

.globl _tramp_mp_main_hart_done
_tramp_mp_main_hart_done:
  .8byte 0

.balign 16
_tramp_mp_init_stack:
  .space 2048
_tramp_mp_init_stack_top:
