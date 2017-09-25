INTERFACE [ux]:

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
IMPLEMENTATION [ux]:

#include "emulation.h"
#include "kip.h"
#include "kmem.h"
#include "vmem_alloc.h"

IMPLEMENT static inline
Unsigned32
Utcb_init::utcb_segment()
{ return 7; }	// RPL=3, TI=LDT, Index=0

IMPLEMENT
void
Utcb_init::init()
{
  Emulation::modify_ldt (0,					// entry
			 Mem_layout::Utcb_ptr_page_user,	// address
			 sizeof (Address) - 1);			// limit
}
