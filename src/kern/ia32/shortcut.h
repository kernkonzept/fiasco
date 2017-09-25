#ifndef SHORTCUT_H
#define SHORTCUT_H

#include "globalconfig.h"

// stackframe structure
#ifdef CONFIG_BIT32
#define REG_ECX
#define REG_EDX	(1*4)
#define REG_ESI	(2*4)
#define REG_EDI	(3*4)
#define REG_EBX	(4*4)
#define REG_EBP	(5*4)
#define REG_EAX	(6*4)
#define REG_EIP (7*4)
#define REG_CS	(8*4)
#define REG_EFL	(9*4)
#define REG_ESP	(10*4)
#define REG_SS	(11*4)

#else /* 64bit */

#define REG_R15
#define REG_R14 (1*8)
#define REG_R13 (2*8)
#define REG_R12 (3*8)
#define REG_R11 (4*8)
#define REG_R10 (5*8)
#define REG_R9  (6*8)
#define REG_R8  (7*8)
#define REG_RCX (8*8)
#define REG_RDX	(9*8)
#define REG_RSI	(10*8)
#define REG_RDI	(11*8)
#define REG_RBX	(12*8)
#define REG_RBP	(13*8)
#define REG_RAX	(14*8)
#define REG_RIP (15*8)
#define REG_CS	(16*8)
#define REG_RFL	(17*8)
#define REG_RSP	(18*8)
#define REG_SS	(19*8)

#endif

#if defined(CONFIG_JDB) && defined(CONFIG_JDB_ACCOUNTING)

#define CNT_CONTEXT_SWITCH	incl (VAL__MEM_LAYOUT__TBUF_STATUS_PAGE+ \
				    OFS__TBUF_STATUS__KERNCNTS)
#define CNT_ADDR_SPACE_SWITCH	incl (VAL__MEM_LAYOUT__TBUF_STATUS_PAGE+ \
				    OFS__TBUF_STATUS__KERNCNTS + 4)
#define CNT_SHORTCUT_FAILED	incl (VAL__MEM_LAYOUT__TBUF_STATUS_PAGE+ \
				    OFS__TBUF_STATUS__KERNCNTS + 8)
#define CNT_SHORTCUT_SUCCESS	incl (VAL__MEM_LAYOUT__TBUF_STATUS_PAGE+ \
				    OFS__TBUF_STATUS__KERNCNTS + 12)
#define CNT_IOBMAP_TLB_FLUSH	incl (VAL__MEM_LAYOUT__TBUF_STATUS_PAGE+ \
				    OFS__TBUF_STATUS__KERNCNTS + 40)

#else

#define CNT_CONTEXT_SWITCH
#define CNT_ADDR_SPACE_SWITCH
#define CNT_SHORTCUT_FAILED
#define CNT_SHORTCUT_SUCCESS
#define CNT_IOBMAP_TLB_FLUSH

#endif

#endif
