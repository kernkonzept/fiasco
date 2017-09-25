#include "globalconfig.h"
#include "idt_init.h"

.macro	EXCEPTION n
	GATE_ENTRY	\n, 1f, 0x0e
1:	pushl	$(0)
	pushl	$(\n)
	jmp	2f
.endm

.macro	EXCEP_USR n
	GATE_ENTRY	\n, 1f, 0x6e
1:	pushl	$(0)
	pushl	$(\n)
	jmp	2f
.endm

.macro	EXCEP_ERR n
	GATE_ENTRY \n, 1f, 0x0e
1:	pushl	$(\n)
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
GATE_ENTRY	0x08, 0x20 ,0x05
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

GATE_INITTAB_END

2:	pusha
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
	movl	%ss,%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%esp,%eax
	call	trap_dump_panic

