IMPLEMENTATION [sparc]:

#include "mem_layout.h"
#include "config.h"
#include <cstring>

#include <cstdio>

IMPLEMENT
void
Utcb_init::init()
{
  // Utcb_ptr_page is physical address
  printf("do we need a utcb pointer page? %x\n", Mem_layout::Utcb_ptr_page);
  if (0)
    memset((void*)Mem_layout::Utcb_ptr_page, 0, Config::PAGE_SIZE);
}

PUBLIC static inline
void
Utcb_init::init_ap(Cpu const &)
{}

