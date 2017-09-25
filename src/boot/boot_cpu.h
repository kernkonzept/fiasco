#ifndef BOOT_CPU_H
#define BOOT_CPU_H

#include "types.h"

void base_paging_init (void);
void base_map_physical_memory_for_kernel ();
void base_cpu_setup (void);

#endif
