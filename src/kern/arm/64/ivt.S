/* -*- asm -*- */

#include "globalconfig.h"
#include "config_tcbsize.h"
#include "tcboffset.h"
#include "asm_entry.h"

#ifdef CONFIG_CPU_VIRT
#define EL(x) x##_el2
#else
#define EL(x) x##_el1
#endif

#define EF_ERETW  ( 0 * 8)
#define EF_KSP    (32 * 8)
#define EF_ESR    (33 * 8)
#define EF_PFA    (34 * 8)
#define EF_SP     (35 * 8)
#define EF_PC     (36 * 8)
#define EF_PSTATE (37 * 8)
#define EF_SIZE   (38 * 8)

.macro save_gprs no_78=0
	stp	xzr, x0,   [sp]
	stp	x1, x2,    [sp,  #16]
	stp	x3, x4,    [sp,  #32]
	stp	x5, x6,    [sp,  #48]
	.if \no_78 == 0
		stp	x7, x8,    [sp,  #64]
	.endif
	stp	x9, x10,   [sp,  #80]
	stp	x11, x12,  [sp,  #96]
	stp	x13, x14,  [sp, #112]
	stp	x15, x16,  [sp, #128]
	stp	x17, x18,  [sp, #144]
	stp	x19, x20,  [sp, #160]
	stp	x21, x22,  [sp, #176]
	stp	x23, x24,  [sp, #192]
	stp	x25, x26,  [sp, #208]
	stp	x27, x28,  [sp, #224]
	stp	x29, x30,  [sp, #240]
.endm

.macro restore_gprs freg=sp
	.if \freg != x0
		ldr	x0,        [\freg,   #8]
	.endif
	ldp	x1, x2,    [\freg,  #16]
	ldp	x3, x4,    [\freg,  #32]
	ldp	x5, x6,    [\freg,  #48]
	ldp	x7, x8,    [\freg,  #64]
	ldp	x9, x10,   [\freg,  #80]
	ldp	x11, x12,  [\freg,  #96]
	ldp	x13, x14,  [\freg, #112]
	ldp	x15, x16,  [\freg, #128]
	ldp	x17, x18,  [\freg, #144]
	ldp	x19, x20,  [\freg, #160]
	ldp	x21, x22,  [\freg, #176]
	ldp	x23, x24,  [\freg, #192]
	ldp	x25, x26,  [\freg, #208]
	ldp	x27, x28,  [\freg, #224]
	ldp	x29, x30,  [\freg, #240]
.endm

.macro load_eret_state freg=sp
	ldp	x3, x4, [\freg, #EF_PC]
	msr	EL(ELR), x3
	msr	EL(SPSR), x4
.endm


.section	.text.excp, "xa"

.type	leave_by_trigger_exception, function
.global	leave_by_trigger_exception
leave_by_trigger_exception:
	mov	x1, #(0x3e << 26)
	str	x1,  [x0, #EF_ESR]
	b	slowtrap_entry

.type	fast_ret_from_irq, function
.global	fast_ret_from_irq
.type	__return_from_user_invoke, function
.global	__return_from_user_invoke
.type	__iret, function
.global	__iret
__iret:
	load_eret_state sp
	add	sp, sp, #EF_SIZE
	eret

.Lret_from_exception:
	ldr	x0, [sp, #EF_KSP]
__return_from_user_invoke:
	ldr	x1, [x0, #EF_ERETW]
	cbnz	x1, 1f // handle continuations
	mov	sp, x0
fast_ret_from_irq:
	mrs x5, daif
	cmp x5, #0x3c0
	b.eq 99f
	brk 0x400
99:
	load_eret_state sp
	restore_gprs
	add	sp, sp, #EF_SIZE
	eret

1:	str	xzr, [x0, #EF_ERETW] // reset continuation
	br	x1                   // handle continuation in x1

.type	vcpu_resume, function
.global	vcpu_resume
vcpu_resume:
	add	sp, x1, #EF_SIZE
	ldr	x3, [x0, #EF_SP]
	msr	SP_EL0, x3
	load_eret_state x0
	restore_gprs x0
	ldr	x0, [x0, #8]
	eret


.global kern_kdebug_ipi_entry
kern_kdebug_ipi_entry:
	brk #2
	ret

.align 3
.global	sys_call_table
sys_call_table:
	.8byte sys_ipc_wrapper

.Lgeneric_entry:
	save_gprs 1
	mrs	x3, EL(ELR)
	mrs	x4, EL(SPSR)
	stp	x3, x4, [sp, #EF_PC]
	mov	x0, sp
	str	x0, [sp, #EF_KSP]
	adr	x30, .Lret_from_exception
	br	x7

.macro ENTRY func
	sub	sp, sp, #EF_SIZE
	stp	x7, x8,    [sp,  #64]
	adr	x7, \func
	b .Lgeneric_entry
.endm

.macro ENTRY_ESR func
	sub	sp, sp, #EF_SIZE
	stp	x7, x8,    [sp,  #64]
	mrs	x8, EL(ESR)
	str	x8, [sp, #EF_ESR]
	adr	x7, \func
	b .Lgeneric_entry
.endm

.Lirq_entry:
	sub	sp, sp, #EF_SIZE
	save_gprs
	mrs	x3, EL(ELR)
	mrs	x4, EL(SPSR)
	stp	x3, x4, [sp, #EF_PC]
	mov	x0, sp
	str	x0, [sp, #EF_KSP]
	adr	x30, .Lret_from_exception
	b	irq_handler


.section	.text.ivt,"xa"
.align 11
.global exception_vector
exception_vector:
	# from el1 with sp_el0
	.align 7
	ENTRY arm_kernel_sync_entry
	b .

	.align 7
	ENTRY_ESR call_nested_trap_handler
	b .

	.align 7
	ENTRY_ESR call_nested_trap_handler
	b .

	.align 7
	ENTRY_ESR call_nested_trap_handler
	b .

	# from el1 with sp_el1
	.align 7
	ENTRY arm_kernel_sync_entry
	b .

	.align 7 // IRQ
	b	.Lirq_entry

	.align 7
	b	.Lirq_entry

	.align 7
	b	.Lirq_entry

	# from el0 aarch64
	.align 7
	ENTRY arm_esr_entry

	.align 7 // IRQ
	b	.Lirq_entry

	.align 7
	b	.Lirq_entry

	.align 7
	b	.Lirq_entry

	# from el0 aarch32
	.align 7
	ENTRY arm_esr_entry
	b .

	.align 7
	b	.Lirq_entry
	b .

	.align 7
	b	.Lirq_entry
	b .

	.align 7
	b	.Lirq_entry
	b .

	.align 7
