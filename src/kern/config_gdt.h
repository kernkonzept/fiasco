#ifndef CONFIG_GDT_H
#define CONFIG_GDT_H

#include "globalconfig.h"

// We want to be Hazelnut compatible, because we want to use the same
// sysenter IPC bindings. When doing sysenter they push 0x1b and the
// returning EIP. The 0x1b value is used for small address spaces where
// the code segment has to be reloaded after a sysexit was performed
// (sysexit loads a flat 4GB segment into CS and SS).
//
// Furthermore, the order of GDT_CODE_KERNEL, GDT_DATA_KERNEL, GDT_CODE_USER,
// GDT_DATA_USER is important since the sysenter/sysexit instruction pair
// assumes this layout (see Intel Reference Manual about sysenter/sysexit)

#ifdef CONFIG_AMD64
# define NEXT_SYS_DESC(x) (x + 16)
#else
# define NEXT_SYS_DESC(x) (x + 8)
#endif
#define NEXT_SEG_DESC(x) (x + 8)

#define GDT_CODE_KERNEL		(0x08)		// #1
#define GDT_DATA_KERNEL		NEXT_SEG_DESC(GDT_CODE_KERNEL)	// #2

/* The next three segments need to be consecutive for sysret */
#define GDT_CODE_USER32		NEXT_SEG_DESC(GDT_DATA_KERNEL)	// #3
#define GDT_DATA_USER		NEXT_SEG_DESC(GDT_CODE_USER32)	// #4
#define GDT_CODE_USER64		NEXT_SEG_DESC(GDT_DATA_USER)	// #5
#define GDT_RESERVED		NEXT_SEG_DESC(GDT_CODE_USER64)	// #6

#define GDT_LDT			NEXT_SYS_DESC(GDT_RESERVED)	// #7

#define GDT_UTCB		NEXT_SYS_DESC(GDT_LDT)		// #8 segment for UTCB pointer
#define GDT_USER_ENTRY1		NEXT_SEG_DESC(GDT_UTCB)		// #9
#define GDT_USER_ENTRY2		NEXT_SEG_DESC(GDT_USER_ENTRY1)	// #10
#define GDT_USER_ENTRY3		NEXT_SEG_DESC(GDT_USER_ENTRY2)	// #11
#define GDT_USER_ENTRY4		NEXT_SEG_DESC(GDT_USER_ENTRY3)	// #12
#define GDT_TSS			NEXT_SEG_DESC(GDT_USER_ENTRY4)	// #13: hardware task segment
#define GDT_TSS_DBF		NEXT_SYS_DESC(GDT_TSS)		// #14: tss for dbf handler
#define GDT_MAX			NEXT_SEG_DESC(GDT_TSS_DBF)

#ifdef CONFIG_IA32
#define GDT_CODE_USER GDT_CODE_USER32
#else
#define GDT_CODE_USER GDT_CODE_USER64
#endif

#endif // CONFIG_GDT_H
