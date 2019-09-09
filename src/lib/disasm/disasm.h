#pragma once

#include "types.h"
#include "globalconfig.h"
#include "jdb_types.h"

class Space;

typedef int (*Peek_task)(Jdb_address addr, void *value, int width);
typedef int (*Is_adp_mem)(Jdb_address addr);

extern int
disasm_bytes(char *buffer, unsigned len, Jdb_address addr,
             int show_intel_syntax,
             Peek_task peek_task, Is_adp_mem adp_mem);

void disasm_init(void *ptr, unsigned size);
