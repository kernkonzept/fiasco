/* -*- c -*- */
#include "config_gdt.h"
#include "tcboffset.h"
#include "linking.h"
#include "tramp-realmode.h"

REALMODE_SECTION

  /*
   * Pseudo_descriptor for the initial GDT
   */
.global _realmode_init_gdt_pdesc
   	.align 4
_realmode_init_gdt_pdesc:
	.word 0x1f
	.long _realmode_init_gdt
	.word 0

.global _realmode_startup_pdbr
	.align 8
_realmode_startup_pdbr:
	.quad 0x00000000

  /*
   * Initial GDT, used for switch to protected mode, will be
   * used only until startup sequence (from this point we use
   * the OS provided GDT).
   */
   	.align 8
_realmode_init_gdt:
	.long 0x00000000, 0x00000000 /* dummy                */
	.long 0x0000FFFF, 0x00CF9A00 /* r-x/0..4GB/PL0/32bit */
	.long 0x0000FFFF, 0x00CF9200 /* rw-/0..4GB/PL0/32bit */
	.long 0x0000FFFF, 0x00AF9B00 /* r-x/0..4GB/PL0/64bit */



