#pragma once

#include "types.h"
#include "globalconfig.h"
#include "jdb_types.h"

typedef int (*Peek_task)(Jdb_address addr, void *value, int width);
typedef int (*Is_adp_mem)(Jdb_address addr);

/**
 * Disassemble a single instruction at address `addr`, print up to `pintlen`
 * characters and return the determined length of the instruction.
 *
 * \param printlen           The maximum number of characters to print. If 0,
 *                           don't print anything and just return the length of
 *                           the instruction.
 * \param clreol             If true, also perform "\033[K" to tell the terminal
 *                           to clear the rest of the line.
 * \param addr               The address of the instruction to disassemble.
 * \param show_intel_syntax  On x86/amd64, this parameter switches between the
 *                           Intel syntax (true) and the AT&T syntax (false).
 * \param show_arm_thumb     On ARM, this parameter enables (true) or disables
 *                           (false) thumb instructions.
 * \param peek_task          Function callback to peek memory.
 * \param adp_mem            Function callback to determine if the memory at
 *                           a specific address is "adapter memory" (should
 *                           not be read).
 * \return The length of the disassembled instruction.
 */
extern int
disasm_bytes(unsigned printlen, bool clreol, Jdb_address addr,
             [[maybe_unused]] bool show_intel_syntax,
             [[maybe_unused]] bool show_arm_thumb,
             Peek_task peek_task, Is_adp_mem adp_mem);

/**
 * Initialize the disassembler subsystem.
 */
void disasm_init(void *ptr, unsigned size);
