/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2015 */

#ifndef CS_UTILS_H
#define CS_UTILS_H

#if defined(CAPSTONE_HAS_OSXKERNEL)
#include <libkern/libkern.h>
#else
#include <stddef.h>
#include "include/capstone/capstone.h"
#endif
#include "cs_priv.h"

// threshold number, so above this number will be printed in hexa mode
#define HEX_THRESHOLD 9

// map instruction to its characteristics
typedef struct insn_map {
	unsigned short id;
	unsigned short mapid;
#ifndef CAPSTONE_DIET
# if defined(CAPSTONE_HAS_ARM64) // Fiasco: optimize space!
	uint16_t regs_use[2]; // list of implicit registers used by this instruction
	uint16_t regs_mod[2]; // list of implicit registers modified by this instruction
	unsigned char groups[4]; // list of group this instruction belong to
# elif defined(CAPSTONE_HAS_ARM) // Fiasco: optimize space!
	uint16_t regs_use[4]; // list of implicit registers used by this instruction
	uint16_t regs_mod[3]; // list of implicit registers modified by this instruction
	unsigned char groups[6]; // list of group this instruction belong to
# elif defined(CAPSTONE_HAS_X86) // Fiasco: optimize space!
	uint16_t regs_use[9]; // list of implicit registers used by this instruction
	uint16_t regs_mod[17]; // list of implicit registers modified by this instruction
	unsigned char groups[4]; // list of group this instruction belong to
# elif defined(CAPSTONE_HAS_MIPS) // Fiasco: optimize space!
	uint16_t regs_use[3]; // list of implicit registers used by this instruction
	uint16_t regs_mod[6]; // list of implicit registers modified by this instruction
	unsigned char groups[8]; // list of group this instruction belong to
# else
#  warning optimize insn_map.reg_use/mod/groups
	uint16_t regs_use[12]; // list of implicit registers used by this instruction
	uint16_t regs_mod[20]; // list of implicit registers modified by this instruction
	unsigned char groups[8]; // list of group this instruction belong to
# endif
	bool branch;	// branch instruction?
	bool indirect_branch;	// indirect branch instruction?
#endif
} insn_map;

// look for @id in @m, given its size in @max. first time call will update @cache.
// return 0 if not found
unsigned short insn_find(const insn_map *m, unsigned int max, unsigned int id, unsigned short **cache);

// map id to string
typedef struct name_map {
	unsigned int id;
	const char *name;
} name_map;

// map a name to its ID
// return 0 if not found
int name2id(const name_map* map, int max, const char *name);

// map ID to a name
// return NULL if not found
const char *id2name(const name_map* map, int max, const unsigned int id);

// count number of positive members in a list.
// NOTE: list must be guaranteed to end in 0
#define count_positive(list) count_positive_max(list, ARR_SIZE(list))
#define count_positive8(list) count_positive8_max(list, ARR_SIZE(list))
unsigned int count_positive_max(const uint16_t *list, unsigned int max);
unsigned int count_positive8_max(const unsigned char *list, unsigned int max);

#define ARR_SIZE(a) (sizeof(a)/sizeof(a[0]))
#define MATRIX_SIZE(a) (sizeof(a[0])/sizeof(a[0][0]))

char *cs_strdup(const char *str);

#define MIN(x, y) ((x) < (y) ? (x) : (y))

// we need this since Windows doesn't have snprintf()
int cs_snprintf(char *buffer, size_t size, const char *fmt, ...);

#define CS_AC_IGNORE (1 << 7)

// check if an id is existent in an array
bool arr_exist8(unsigned char *arr, unsigned char max, unsigned int id);

bool arr_exist(uint16_t *arr, unsigned char max, unsigned int id);

#endif

