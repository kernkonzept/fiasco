#pragma once

#ifndef __mips64

#define ASM_WORD_BYTES 4
#define ASM_NARGSAVE 4

#ifndef __ASSEMBLER__
# define ASM_MFC0   "mfc0"
# define ASM_MTC0   "mtc0"
# define ASM_ADDU   "addu"
# define ASM_ADDIU  "addiu"
# define ASM_LL     "ll"
# define ASM_SC     "sc"
# define ASM_L      "lw"
# define ASM_S      "sw"
# define ASM_LA     "la"
# define ASM_LI     "li"
#else
# define ASM_MFC0   mfc0
# define ASM_MTC0   mtc0
# define ASM_ADDU   addu
# define ASM_ADDIU  addiu
# define ASM_LL     ll
# define ASM_SC     sc
# define ASM_L      lw
# define ASM_S      sw
# define ASM_LA     la
# define ASM_LI     li
#endif

#else

#define ASM_WORD_BYTES 8
#define ASM_NARGSAVE 0

#ifndef __ASSEMBLER__
# define ASM_MFC0   "dmfc0"
# define ASM_MTC0   "dmtc0"
# define ASM_ADDU   "daddu"
# define ASM_ADDIU  "daddiu"
# define ASM_LL     "lld"
# define ASM_SC     "scd"
# define ASM_L      "ld"
# define ASM_S      "sd"
# define ASM_LA     "dla"
# define ASM_LI     "dli"
#else
# define ASM_MFC0   dmfc0
# define ASM_MTC0   dmtc0
# define ASM_ADDU   daddu
# define ASM_ADDIU  daddiu
# define ASM_LL     lld
# define ASM_SC     scd
# define ASM_L      ld
# define ASM_S      sd
# define ASM_LA     dla
# define ASM_LI     dli
#endif

#endif
