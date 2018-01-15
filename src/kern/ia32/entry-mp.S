#include "idt_init.h"
#include "tcboffset.h"
#include <low_level.h>

	.section ".entry.text.ipi", "ax", @progbits
	.p2align 4
	.globl entry_ipi_remote_request
entry_ipi_remote_request:
	SWITCH_TO_KERNEL_CR3 0
	SAVE_SCRATCH
	call	handle_remote_cpu_requests
	RESTORE_SCRATCH
	SAFE_IRET

	.section ".entry.text.ipi", "ax", @progbits
	.p2align 4
	.globl entry_ipi
entry_ipi:
	SWITCH_TO_KERNEL_CR3 0
	SAVE_SCRATCH
	call	ipi_remote_call
	RESTORE_SCRATCH
	SAFE_IRET

	.section ".entry.text.debug_ipi", "ax", @progbits
	.p2align 4
	.globl entry_debug_ipi
entry_debug_ipi:
	SWITCH_TO_KERNEL_CR3 0
	push	$(0)
	push	$(0xee)
	save_all_regs
	jmp slowtraps
