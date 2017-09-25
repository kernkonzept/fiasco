//---------------------------------------------------------------------------
IMPLEMENTATION [ppc32]:

#include "config.h"
#include "panic.h"
#include "warn.h"
#include "mem.h"

IMPLEMENT
void Vmem_alloc::init()
{
  NOT_IMPL_PANIC;
}

IMPLEMENT
void *Vmem_alloc::page_alloc(void * address, Zero_fill zf, unsigned /*mode*/)
{
  if(zf == ZERO_FILL)
    Mem::memset_mwords((unsigned long *)address, 0, Config::PAGE_SIZE >> 2);
  return address;
}

IMPLEMENT
void *Vmem_alloc::page_unmap(void * /*page*/)
{
  NOT_IMPL_PANIC;
  return NULL;
}

