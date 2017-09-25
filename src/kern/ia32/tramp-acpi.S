/* -*- c -*- */
#include "config_gdt.h"
#include "linking.h"
#include "regdefs.h"
#include "tramp-realmode.h"

#ifdef CONFIG_AMD64
# define BX rbx
#else
# define BX ebx
#endif

#define SET_VIDEO_MODE 0
#define WAK_STS 0x8000
#define SLEEP_EN 0x2000

/* (c) 2013 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com> */

/* 
 * Acpi wakeup code.
 * 
 * We are thrown out of sleep directly into real mode. Therefore we must be
 * below 1MB! We pack it into the .realmode_tramp section, which starts at
 * 0x1000. 
 */
  .section .realmode_tramp , "awx", @progbits

  .code16
  .align 16
  .globl _tramp_acpi_wakeup
_tramp_acpi_wakeup:
  cli
  cld
  RM_LOAD_SEGMENT_BASE
  movw  $RM_OFFS(_acpi_rm_stack_top), %sp

  /* Clear all CPU flags */
  pushl	$0
  popfl

#if SET_VIDEO_MODE
  /*
   * Mask everything in the PIC for some reason the PIT timer is
   * unmasked which causes spurious calls to IDT vecotor zero at least
   * during video mode reset.
   */
  mov $0xff, %al
  outb %al, $0xa1
  outb %al, $0x21

  /* IDT needed for BIOS */
  lidtw RM_OFFS(rm_idt_desc)

  cld
  cli

  /* Call the bios */
  lcallw $0xc000,$3
  RM_LOAD_SEGMENT_BASE

  cld
  cli

  /* set graphics mode */
  mov  $3, %ax
  int  $0x10

  RM_LOAD_SEGMENT_BASE
#endif

  ENTER_PROTECTED_MODE
  /* 32bit code follows */
  ENABLE_PAGING cr0=_cr0 cr3=_cr3 cr4=_cr4 gdt=_gdt

  lidt _idt
  lldt _ldt

#if 0
  movw _fs, %ax
  movw %ax, %fs
  movw _gs, %ax
  movw %ax, %gs
#endif

  /* restore general purpose registers */
#ifdef CONFIG_AMD64
  mov _sp, %rsp
  pop %rbx
  pop %rbp
  pop %r8
  pop %r9
  pop %r10
  pop %r11
  pop %r12
  pop %r13
  pop %r14
  pop %r15
  popfq
#else
  mov _sp, %esp
  pop %ebx
  pop %esi
  pop %edi
  pop %ebp
  popfl
#endif

  /* we cleared the busy flag in C++ code before suspend, so its save to load the TR here */
  ltr _tr
  movl $0, %eax // return 0 (success)
  ret                   // return from function "acpi_save_cpu_and_suspend"

  /* small stack needed for bios call */
  .align 16
  .space 0x90, 0
_acpi_rm_stack_top:

#if SET_VIDEO_MODE
  /* Real mode IDT descriptor for bios IDT at physical address zero. */
.align 16
rm_idt_desc:
  .word	0xffff
  .long	0
#endif


_wakeup_header:
_cr0:   .quad 0
_cr3:   .quad 0
_cr4:   .quad 0
_sp:    .quad 0
_gdt:   .quad 0
        .quad 0
_idt:   .quad 0
        .quad 0
_ldt:   .quad 0
        .quad 0
#if 0
_fs:    .word 0
_gs:    .word 0
#endif

_tr:    .quad 0 /* task register */
        .quad 0

// normal code called from C++

.section .text.acpi_suspend , "ax", @progbits
  .align 16, 0x90
  .globl acpi_save_cpu_and_suspend
acpi_save_cpu_and_suspend:
  /**
   * int FIASCO_FASTCALL acpi_save_cpu_and_suspend(Unsigned32 sleep_type,
   *                                               Unsigned32 pm1_control_block,
   *                                               Unsigned32 pm1_event_block);
   * sleep_type = sleep_type_a | sleep_type_b
   * pm1_control_block = (pm1b_cntl_blk << 16) | pm1a_cntl_blk
   * pm1_event_block = (pm1b_evt_blk << 16) | pm1b_evt_blk
   * Called from do_suspend_system in platform_control-acpi_sleep.cpp
   * Function will return 1 on error (suspend failed).
   * It will return 0 after system wakeup (suspend succeeded).
   */

#ifdef CONFIG_AMD64
  pushfq
  push %r15
  push %r14
  push %r13
  push %r12
  push %r11
  push %r10
  push %r9
  push %r8
  push %rbp
  push %rbx
  mov %rsp, _sp
#else
  pushfl
  push %ebp
  push %edi
  push %esi
  push %ebx
  mov %esp, _sp
#endif

  /* save control registers */
  mov %cr0, %BX
  mov %BX, _cr0
  mov %cr3, %BX
  mov %BX, _cr3
  mov %cr4, %BX
  mov %BX, _cr4

#if 0
  movw %fs, %bx
  movw %bx, _fs
  movw %gs, %bx
  movw %bx, _gs
#endif

  str _tr
  /* save descriptor table (registers) */
  sgdt _gdt
  sidt _idt
  sldt _ldt

  wbinvd

  /* 32bit: eax = sleep_type,
            edx = (pm1b_cntl << 16) | pm1a_cntl,
            ecx = (pm1b_sts  << 16) | pm1a_sts */
  /* 64bit: rdi = sleep_type,
            rsi = (pm1b_cntl << 16) | pm1a_cntl,
            rdx = (pm1b_sts  << 16) | pm1a_sts */

#ifdef CONFIG_AMD64
  mov %rdx, %rcx
#else
  mov %eax, %edi
  mov %edx, %esi
#endif

  /* 32/64bit: di = sleep_type,
              esi = (pm1b_cntl << 16) | pm1a_cntl,
              ecx = (pm1b_sts  << 16) | pm1a_sts */

  mov %edi, %ebx
  shl $10, %ebx
  mov %esi, %edx

  in  %dx,      %ax   // read from dx into ax
  or  %bx,      %ax
  or  $SLEEP_EN, %ax
  out %ax,      %dx  // write ax to dx

  shr $16, %esi
  cmp $0, %si
  je  out

  mov %edi, %ebx
  shr $8, %ebx
  shl $10, %ebx
  mov %esi, %edx

  in  %dx,      %ax
  or  %bx,      %ax
  or  $SLEEP_EN, %ax
  out %ax,      %dx

  // platform will now enter S3, which may or may not take some time
  // until then we loop on the WAK_STS bit as implemented in
  // pkg/acpica/lib-acpi/src/acpica/components/hardware/hwsleep.c
  // (AcpiHwLegacySleep)
out:
  /* 32/64 bit ecx = (pm1b_sts << 16) | pm1a_sts */
  mov %ecx, %edx
  in  %dx, %ax
  and $WAK_STS, %ax
  mov %ax, %bx
  shr $16, %edx
  in  %dx, %ax
  and $WAK_STS, %ax
  or  %bx, %ax
  cmp $WAK_STS, %ax
  jnz out

  /* if we reach this code, suspend failed (this should *never* happen) */
  /* otherwise the machine started up from from _tramp_acpi_wakeup */
  movl $1, %eax
  ret

