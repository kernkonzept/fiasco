IMPLEMENTATION [arm && !arm_v6plus]:

#include "mem_layout.h"
#include "paging.h"
#include "panic.h"
#include "vmem_alloc.h"

IMPLEMENT
void
Utcb_init::init()
{
  if (!Vmem_alloc::page_alloc(reinterpret_cast<void *>(Mem_layout::Utcb_ptr_page),
                              Vmem_alloc::ZERO_FILL, Vmem_alloc::User))
    panic("UTCB pointer page allocation failure");
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

IMPLEMENT inline ALWAYS_INLINE void Utcb_init::init() {}
