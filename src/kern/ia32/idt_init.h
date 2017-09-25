#include <globalconfig.h>

#ifndef IDT_INIT
#define IDT_INIT

#define FIASCO_IDT_MAX  0x100
#define APIC_IRQ_BASE   (FIASCO_IDT_MAX - 0x08)

#ifdef ASSEMBLER

.macro	GATE_INITTAB_BEGIN name
	.pushsection ".initcall.data", "aw", @progbits
	.globl	\name
	.align 8
\name:
        .popsection
.endm

#ifdef CONFIG_BIT32

.macro	GATE_ENTRY n,name,type
	.pushsection ".initcall.data", "aw", @progbits
	.long	\name
	.word	\n
	.word	\type
	.popsection
.endm

.macro	GATE_INITTAB_END
	.pushsection ".initcall.data"
	.long	0
	.popsection
.endm

#else
.macro	GATE_ENTRY n, name, type
	.pushsection ".initcall.data"
	.quad	\name
	.word	\n
	.word	\type
	.popsection
.endm

.macro	GATE_INITTAB_END
	.pushsection ".initcall.data"
	.quad	0
	.popsection
.endm
#endif

#define SEL_PL_U	0x03
#define SEL_PL_K	0x00

#define ACC_TASK_GATE	0x05
#define ACC_INTR_GATE	0x0e
#define ACC_TRAP_GATE	0x0f
#define ACC_PL_U	0x60
#define ACC_PL_K	0x00

#else // !ASSEMBLER

#include "l4_types.h"

#ifdef CONFIG_BIT32
class Idt_init_entry
{
public:
  Unsigned32  entry;
  Unsigned16  vector;
  Unsigned16  type;
} __attribute__((packed));
#else
class Idt_init_entry
{
public:
  Unsigned64  entry;
  Unsigned16  vector;
  Unsigned16  type;
} __attribute__((packed));
#endif

extern Idt_init_entry idt_init_table[];

#endif

#endif
