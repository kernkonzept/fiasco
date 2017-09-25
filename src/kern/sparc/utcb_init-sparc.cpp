IMPLEMENTATION [sparc]:

#include "mem_layout.h"
#include "config.h"
#include <cstring>


IMPLEMENT
void
Utcb_init::init()
{
  //Utcb_ptr_page is physical address
  memset((void*)Mem_layout::Utcb_ptr_page, 0, Config::PAGE_SIZE);
}

PUBLIC static inline
void
Utcb_init::init_ap(Cpu const &)
{}

