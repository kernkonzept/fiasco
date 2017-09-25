INTERFACE [ia32 || amd64]:

class Cpu;

EXTENSION class Utcb_init
{
public:
  /**
   * Value for GS and FS.
   * @return Value the GS and FS register has to be loaded with when
   *         entering user mode.
   */
  static Unsigned32 utcb_segment();
};

//-----------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:

#include <cstdio>
#include "gdt.h"
#include "paging.h"
#include "panic.h"
#include "space.h"
#include "vmem_alloc.h"

IMPLEMENT static inline NEEDS ["gdt.h"]
Unsigned32
Utcb_init::utcb_segment()
{ return Gdt::gdt_utcb | Gdt::Selector_user; }


IMPLEMENT inline
void
Utcb_init::init()
{}
