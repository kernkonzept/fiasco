#include "globalconfig.h"
#include "idt_init.h"

.macro	EXCEPTION n
	GATE_ENTRY	\n, 1f, 0x0e
1:	push	$(0)
	push	$(\n)
	jmp	2f
.endm

.macro	EXCEP_USR n
	GATE_ENTRY	\n, 1f, 0x6e
1:	push	$(0)
	push	$(\n)
	jmp	2f
.endm

.macro	EXCEP_ERR n
	GATE_ENTRY \n, 1f, 0x0e
1:	push	$(\n)
	jmp	2f
.endm

GATE_INITTAB_BEGIN boot_idt_inittab

EXCEPTION	0x00
EXCEPTION	0x01
EXCEPTION	0x02
EXCEP_USR	0x03
EXCEP_USR	0x04
EXCEP_USR	0x05
EXCEPTION	0x06
EXCEPTION	0x07
EXCEPTION	0x08
EXCEPTION	0x09
EXCEP_ERR	0x0a
EXCEP_ERR	0x0b
EXCEP_ERR	0x0c
EXCEP_ERR	0x0d
EXCEP_ERR	0x0e
EXCEPTION	0x0f
EXCEPTION	0x10
EXCEPTION	0x11
EXCEPTION	0x12
EXCEPTION	0x13
EXCEPTION	0x14
EXCEPTION	0x15
EXCEPTION	0x16
EXCEPTION	0x17
EXCEPTION	0x18
EXCEPTION	0x19
EXCEPTION	0x1a
EXCEPTION	0x1b
EXCEPTION	0x1c
EXCEPTION	0x1d
EXCEPTION	0x1e
EXCEPTION	0x1f

GATE_INITTAB_END

2:
	push	%r15
	push	%r14
	push	%r13
	push	%r12
	push	%r11
	push	%r10
	push	%r9
	push	%r8
	mov	%cr2,%r8
	push	%r8
	push	%rbp
	push	%rsi
	push	%rdi
	push	%rdx
	push	%rcx
	push	%rbx
	push	%rax	
	

	call	trap_dump_panic

