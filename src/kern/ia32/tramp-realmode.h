#pragma once

#include "config_gdt.h"
#include "linking.h"
#include "regdefs.h"

.macro REALMODE_SECTION
	.section .realmode_tramp , "awx", @progbits
.endm

#define RM_OFFS(sym) ((sym) - (FIASCO_MP_TRAMP_PAGE))
#define RM_SEGMENT_BASE ((FIASCO_MP_TRAMP_PAGE) >> 4)

/* clobbers: EAX */
.macro RM_LOAD_SEGMENT_BASE
	/* set segments */
	mov	$RM_SEGMENT_BASE,	%ax
	mov	%ax,	%ds
	mov	%ax,	%es
	mov	%ax,	%ss
.endm

/*
 * These defines must index into _realmode_init_gdt
 */
#define PROT_CS 0x08
#define PROT_DS 0x10
#define LONG_CS 0x18

/* clobbers: EXA */
.macro ENTER_PROTECTED_MODE
	/* switch to protected mode */
	lgdtl	RM_OFFS(_realmode_init_gdt_pdesc)

	movl	%cr0,	 %eax
	orl	$CR0_PE,	%eax
	movl	%eax,	%cr0

	/* reload %cs */
	ljmpl	$PROT_CS,	$1f

	.code32
	.align 4
1:	/* reload ds */
	movw	$PROT_DS,	%ax
	movw	%ax,	%ds
.endm

/* clobbers: EAX, EDX, ECX */
.macro ENABLE_PAGING cr0, cr3, cr4, gdt
	movl	\cr4, %eax
	movl	%eax, %cr4
#ifdef CONFIG_AMD64
	movl	_realmode_startup_pdbr,	%eax
	movl	%eax,	%cr3
	mov $0xc0000080, %ecx
	rdmsr
	or  $0x900,%eax // LME+NXE
	wrmsr
#else
	movl	\cr3,	%eax
	movl	%eax,	%cr3
#endif

	/* this enables paging */
	movl	\cr0, %eax
	movl	%eax, %cr0

#ifdef CONFIG_AMD64
	ljmpl	$LONG_CS, $1f
	.align 4
	.code64
1:
	/* load the 64bit CR3 */
	mov	\cr3, %rax
	mov	%rax, %cr3
#endif
	/* run with paging enabled from here on */
	lgdt	\gdt
#ifndef CONFIG_AMD64
	ljmp	$GDT_CODE_KERNEL, $2f
#endif
2:	movw	$GDT_DATA_KERNEL, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	$0, %ax
	movw	%ax, %fs
	movw	%ax, %gs
.endm




