
.macro entry_psr_save
	mov %psr, %l0
.endm

.macro BAD_TRAP
	ta 0
	nop
	nop
	nop
.endm

//#define TRAP_ENTRY(H) rd %psr, %l0; b H; rd %wim, %l3; nop;
//#define TRAP_ENTRY_INTERRUPT(int_level) mov int_level, %l7; rd %psr, %l0; b _prom_leonbare_irq_entry; rd %wim, %l3;

.macro trap_entry func
	entry_psr_save
	sethi %hi(\func), %l4
	jmp %l4 + %lo(\func)
	nop
.endm

.macro irq_entry irqno
	entry_psr_save
	sethi %hi(do_irq), %l4
	jmp %l4 + %lo(do_irq)
	mov \irqno, %o0
.endm

.section .text.init, "ax"
.global _start
.global __init_stack

/*
 * Trap handler table -> must be aligned to page size
 * as specified by the SPARC v8 manual (p. 31).
 */
.align(0x1000)
_traphandlers:
  BAD_TRAP // 0
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP
  trap_entry(sparc_window_overflow); // 5
  trap_entry(sparc_window_underflow); // 6
  BAD_TRAP
  BAD_TRAP // 0x08
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP // 0x0c
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP // 0x10
  irq_entry(1)
  irq_entry(2)
  irq_entry(3)
  irq_entry(4) // 0x14
  irq_entry(5)
  irq_entry(6)
  irq_entry(7)
  irq_entry(8) // 0x18
  irq_entry(9)
  irq_entry(10)
  irq_entry(11)
  irq_entry(12) // 0x1c
  irq_entry(13)
  irq_entry(14)
  irq_entry(15)
  BAD_TRAP // 0x20
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP // 0x24
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP // 0x28
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP // 0x2c
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP // 0x30
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP // 0x34
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP // 0x38
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP // 0x3c
  BAD_TRAP
  BAD_TRAP
  BAD_TRAP
  /* 0x40 */
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  /* 0x50 */
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  /* 0x60 */
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  /* 0x70 */
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  /* 0x80 */
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  /* 0x90 */
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  /* 0xa0 */
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  /* 0xb0 */
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  /* 0xc0 */
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  /* 0xd0 */
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  /* 0xe0 */
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  /* 0xf0 */
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;
  BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;

_start:

  rd %asr17, %g1

  /*
   * setup trap handler table
   */
  sethi %hi(_traphandlers), %g2
  wr %g2, %tbr

  /* the TBR setup above comes into effect three instructions from now!
   * Right now, we assume that no trap occurs in between.
   */

  /* set stack pointer */
  sethi %hi(__init_stack), %sp
  or %sp, %lo(__init_stack), %sp

  /*
   * setup task with enough space for registers %l0-%l7 and %i0-%i7
   */
  sub %sp, 64, %sp

  sethi %hi(__init_stack), %fp
  or %fp, %lo(__init_stack), %fp

  ba bootstrap_main
  nop

  ta 0
1:
  ba 1b

// ------------------------------------------------------------------------

#define PSR_ET    0x20
#define PSR_PIL15 0xf00

do_irq:
  // %l0: PSR
  // %l1: pc
  // %l2: npc
  // %o0: irq number

  // todo: Check CWP, do window processing if necessary

  
  rd   %psr, %l4        // get PSR
  bset PSR_PIL15, %l4   // disable interrupts
  mov  %l4, %psr        // commit
  nop; nop; nop;        // manual tells us about consecutive writes to psr
  bset PSR_ET, %l4      // enable TE
  mov  %l4, %psr        // commit
  nop; nop; nop;        // ... and we should not write ET and PIL together

  mov %wim, %l3         // l3 = wim
  and %l4, 0x1f, %l4    // l4 = cwp
  mov 1, %l5
  sll %l5, %l4, %l4       // l4 = 1 << l4
  andcc %l3, %l4, %g0
  bne 2f

  // save o7
  mov %o7, %l4

  // save global registers (use std)
  sub %sp, 32, %sp
  st %g1, [ %sp + 0 ]
  st %g2, [ %sp + 4 ]
  st %g3, [ %sp + 8 ]
  st %g4, [ %sp + 12 ]
  st %g5, [ %sp + 16 ]
  st %g6, [ %sp + 20 ]
  st %g7, [ %sp + 24 ]

  // we could supply the irq number here too so that we do not need to check
  // for pending IRQs later on...
  call irq_handler
   nop

  // restore global registers (use ldd)
  ld [ %sp + 0 ], %g1
  ld [ %sp + 4 ], %g2
  ld [ %sp + 8 ], %g3
  ld [ %sp + 12 ], %g4
  ld [ %sp + 16 ], %g5
  ld [ %sp + 20 ], %g6
  ld [ %sp + 24 ], %g7
  add %sp, 32, %sp


  // restore o7
  mov %l4, %o7

  // undo window stuff if we fiddle with it


  mov %l0, %psr
  nop
  nop
  nop

  jmp %l1
  rett  %l2
  nop

2:
  ta 1

// ------------------------------------------

.globl sparc_window_overflow
sparc_window_overflow:
  mov %wim, %l3  /* need to determine new WIM */
  mov %g1, %l7
  srl %l3, 1, %g1

  /*
   * Find out if we are on LEON3 (PSR[24:27] == 3)
   * or on LEON2. For LEON3, we can read the number of
   * register windows from ASR17
   */
  mov %psr, %l4
  srl %l4, 24, %l4
  and %l4, 3, %l4 // should that be 0xf instead of 3?
  subcc %l4, 3, %g0
  bne 1f
  nop

  /*
   * It's a LEON3
   */
  mov %asr17, %l4

  /* calculate new WIM */
  and %l4, 0x1f, %l4
  sll %l3, %l4, %l4
  or %l4, %g1, %g1

  /*
   * The trick here is to move to a valid stack frame
   * and store the register window contents there.
   */
  save
  mov %g1, %wim
  nop; nop; nop

  std %l0, [%sp + 0];
  std %l2, [%sp + 8];
  std %l4, [%sp + 16];
  std %l6, [%sp + 24];
  std %i0, [%sp + 32];
  std %i2, [%sp + 40];
  std %i4, [%sp + 48];
  std %i6, [%sp + 56];

  restore
  mov %l7, %g1
  jmp %l1
  rett %l2

1: ta 0

.globl sparc_window_underflow
sparc_window_underflow:
  mov %wim, %l3  /* need to determine new WIM */
  sll %l3, 1, %l4

  /* Determine LEON version */
  mov %psr, %l5
  srl %l5, 24, %l5
  and %l5, 3, %l5 // shouldn't that be 0xf instead of 3?
  subcc %l5, 3, %g0
  bne 1f
  nop

  mov %asr17, %l5
  and %l5, 0x1f, %l5
  srl %l3, %l5, %l5
  or  %l5, %l4, %l5
  mov %l5, %wim
  nop; nop; nop

  restore             ! Two restores to get into the
  restore             ! window to restore
  ldd   [%sp + 0], %l0;       ! Restore window from the stack
  ldd   [%sp + 8], %l2;
  ldd   [%sp + 16], %l4;
  ldd   [%sp + 24], %l6;
  ldd   [%sp + 32], %i0;
  ldd   [%sp + 40], %i2;
  ldd   [%sp + 48], %i4;
  ldd   [%sp + 56], %i6;
  save                ! Get back to the trap window.
  save

  jmp %l1
  rett %l2

1: ta 0


.previous
.section .bss
.align 16
.space 4096
  __init_stack:
.previous


