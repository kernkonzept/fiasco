INTERFACE [arm]:

#include "kip.h"
#include "mem_layout.h"
#include "paging.h"

class Kmem : public Mem_layout
{
public:
  static Mword is_kmem_page_fault(Mword pfa, Mword error);
  static Mword is_ipc_page_fault(Mword pfa, Mword error);
  static Mword is_io_bitmap_page_fault(Mword pfa);
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "config.h"
#include "mem_unit.h"
#include "paging.h"
#include <cassert>

IMPLEMENT inline
Mword Kmem::is_io_bitmap_page_fault(Mword)
{
  return 0;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

IMPLEMENT inline
Mword Kmem::is_kmem_page_fault(Mword pfa, Mword)
{
  return in_kernel(pfa);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

#include "paging.h"

IMPLEMENT inline
Mword Kmem::is_kmem_page_fault(Mword, Mword ec)
{
  return !PF::is_usermode_error(ec);
}
