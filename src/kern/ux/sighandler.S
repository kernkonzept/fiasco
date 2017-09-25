
/*
 * Signal Handler Trampoline for traced tasks.
 *
 * This code is copied onto the task's trampoline page and runs in the task's
 * address space. Therefore all addressing must be done with virtual addresses
 * of the task.
 *
 * The whole purpose of this exercise is to get a pointer to the signal
 * context of the task from inside the kernel process. The signal handler
 * runs at the bottom of the trampoline page with the top of the trampoline
 * page as signal stack. The handler receives the following 3 parameters:
 * int signal, siginfo_t *, void *context
 * We want the third parameter, relative to the beginning of the trampoline
 * page and store it at offset 256 on the trampoline page so that code and data
 * don't cacheline collide.
 */

#include "tcboffset.h"

.align	16
.globl	_task_sighandler_start
.globl	_task_sighandler_end

_task_sighandler_start:
	movl	$VAL__MEM_LAYOUT__TRAMPOLINE_PAGE, %ecx
					// get trampoline page start address
	movl	12(%esp), %eax		// get context pointer from sighandler
	andl	$0xfff, %eax		// calculate offset into trampolinepage
	movl	%eax, 0x100(%ecx)	// save offset at trampoline page + 256
	addl	$4, %esp		// consume return address
	movl	$173, %eax		// rt_sigreturn
	int	$0x80			// system call
_task_sighandler_end:
