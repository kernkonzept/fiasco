// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_v6plus]:

#include "mem_layout.h"

// See also Sys_call_page::set_utcb_get_code().
IMPLEMENT inline NEEDS["mem_layout.h"]
void
Utcb_support::current(User_ptr<Utcb> const &utcb)
{ *reinterpret_cast<User_ptr<Utcb>*>(Mem_layout::Utcb_ptr_page) = utcb; }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

IMPLEMENT inline
void
Utcb_support::current(User_ptr<Utcb> const &)
{}
