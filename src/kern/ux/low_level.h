#ifndef LOW_LEVEL_H
#define LOW_LEVEL_H

#include "shortcut.h"
#include "tcboffset.h"

/** Some shared macros and stuff shared by the two different 
 *  assembler kernel entry sources.
 */

        .macro save_all_regs
        pusha
        .endm

        /* Reload Linux segments as user might have changed those */
	.macro	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	movw	KERN_DS,%ds
	movw	KERN_ES,%es
	.endm

	.macro	SAVE_STATE
	pushl	%ebp
	pushl	%ebx
	pushl	%edi
	pushl	%esi
	pushl	%edx
	pushl	%ecx
	.endm

	.macro	RESTORE_STATE
	popl	%ecx
	popl	%edx
	popl	%esi
	popl	%edi
	popl	%ebx
	popl	%ebp
	.endm

	.macro	RESTORE_STATE_AFTER_IPC
#ifndef CONFIG_LOCAL_IPC
	addl	$4, %esp
#else
        popl    %ecx
#endif
	popl	%edx
	popl	%esi
	popl	%edi
	popl	%ebx
	addl	$4, %esp
	.endm

	.macro	RESET_THREAD_CANCEL_AT reg
	andl	$~VAL__Thread_cancel, OFS__THREAD__STATE (\reg)
	.endm

	.macro	RESET_THREAD_IPC_MASK_AT reg
	andl	$~VAL__Thread_ipc_mask, OFS__THREAD__STATE (\reg)
	.endm

	.macro	ESP_TO_TCB_AT reg
	movl	%esp, \reg
	andl	$~(THREAD_BLOCK_SIZE - 1), \reg
	.endm


#define SCRATCH_REGISTER_SIZE 12
	.macro	SAVE_SCRATCH
	push	%eax
	push	%ecx
	push	%edx
	.endm

	.macro	RESTORE_SCRATCH
	pop	%edx
	pop	%ecx
	pop	%eax
	.endm

	.macro	IRET_INSN
	iret
	.endm

/** For Kmem::phys_to_virt() emulation. */
#define PHYSMEM_OFFS	$VAL__MEM_LAYOUT__PHYSMEM

#endif //LOW_LEVEL_H
