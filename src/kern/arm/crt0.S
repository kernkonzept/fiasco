/* -*- c -*- */

#include "asm.h"
.section .text.init,#alloc,#execinstr
.type start,#function
ENTRY(_start_kernel)
ENTRY(start)
     
     ldr sp, __init_data;
     bl	__main

     /* never returns */


.type __init_data, %object
__init_data:
.long SYMBOL_NAME(_stack)



.section ".init.data"
.p2align 3
.globl _sstack
_sstack:
	.space	2048
.globl _stack
.type _stack,#object
_stack:
