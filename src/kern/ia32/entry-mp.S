#include "idt_init.h"
#include "tcboffset.h"
#include <low_level.h>

	.p2align 4
	.globl entry_ipi_remote_request
entry_ipi_remote_request:
	SAVE_SCRATCH
	call	handle_remote_cpu_requests
	RESTORE_SCRATCH
	IRET_INSN

	.p2align 4
	.globl entry_ipi
entry_ipi:
	SAVE_SCRATCH
	call	ipi_remote_call
	RESTORE_SCRATCH
	IRET_INSN

	.p2align 4
	.globl entry_debug_ipi
entry_debug_ipi:
	push	$(0)
	push	$(0xee)
	save_all_regs
	jmp slowtraps
