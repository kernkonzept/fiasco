// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !armv6plus]:

#include "mem_layout.h"

IMPLEMENT inline NEEDS["mem_layout.h"]
void
Utcb_support::current(User<Utcb>::Ptr const &utcb)
{ *reinterpret_cast<User<Utcb>::Ptr*>(Mem_layout::Utcb_ptr_page) = utcb; }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && armv6plus]:

IMPLEMENT inline
void
Utcb_support::current(User<Utcb>::Ptr const &)
{}
