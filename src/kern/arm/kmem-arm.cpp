IMPLEMENTATION [arm && !cpu_virt]:

IMPLEMENT inline
bool
Kmem::is_kmem_page_fault(Mword pfa, Mword)
{
  return in_kernel(pfa);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

#include "paging.h"

IMPLEMENT inline NEEDS["paging.h"]
bool
Kmem::is_kmem_page_fault(Mword, Mword ec)
{
  return !PF::is_usermode_error(ec);
}
