#define __ASSEMBLY__
#include <l4/compiler.h>
	
	.text

ENTRY(__crt_dummy__)
ENTRY(_start)
ENTRY(start)
	leal	stack,%esp
	jmp	SYMBOL_NAME(main)

ENTRY(__main)
	ret

        /* MultiBoot header - see multiboot.h.  */
#ifdef __ELF__
        .align  4
#else /* ! __ELF__, that mean a.out assembler */
        .align  2
#endif

ENTRY(_mb_header)
        .long   0x1BADB002              /* magic */
        .long   0x00010000              /* flags: AOUT_KLUDGE */
        .long   - 0x00010000 - 0x1BADB002
        .long   SYMBOL_NAME(_mb_header) /* header_addr */
        .long   SYMBOL_NAME(__crt_dummy__) /* load_addr */
        .long   _edata			/* load_end_addr */
        .long   _end			/* bss_end_addr */
        .long   SYMBOL_NAME(_start)	/* entry */
	
	.bss

	.space	1024
stack:
