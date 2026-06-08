INTERFACE:

#include "entry_frame.h"

EXTENSION class Trap_state : public Entry_frame
{};

EXTENSION struct Trex
{
public:
  Mword ulr;
};

//----------------------------------------------------------------------------
IMPLEMENTATION:

IMPLEMENT inline
void
Trex::set_ipc_upcall()
{ s.cause = Trap_state::C_l4_ipc_upcall; }


IMPLEMENT inline
void
Trap_state::set_pagefault(Mword pfa, Mword error)
{
  this->bad_v_addr = pfa;
  this->cause = error;
}

// MIPS encodes the error code and trapno into the cause register, so we return
// status as error code.

IMPLEMENT inline
Mword
Trap_state::trapno() const
{ return cause; }

IMPLEMENT inline
Mword
Trap_state::error() const
{ return status; }

IMPLEMENT inline
Mword
Trap_state::ip() const
{ return Return_frame::ip(); }

IMPLEMENT inline
void
Trap_state::ip(Mword new_ip)
{ Return_frame::ip(new_ip); }

IMPLEMENT inline
Mword
Trap_state::sp() const
{ return Return_frame::sp(); }

IMPLEMENT inline
void
Trap_state::dump() const
{ Entry_frame::dump(); }

PUBLIC static inline
char const *
Trap_state::exc_code_to_str(Mword cause)
{
  static constinit char const *const exc_codes[] =
  {
    "IRQ",     "TLB Mod",  "TLBL",      "TLBS",
    "AdEL",    "AdES",     "IBE",       "DBE",
    "Sys",     "Bp",       "RI",        "CpU",
    "Ov",      "Tr",       "MSAFPE",    "FPE",
    "Impl1",   "Impl2",    "C2E",       "TLBRI",
    "TLBXI",   "MSADis",   "MDMX",      "WATCH",
    "MCheck",  "Thread",   "DSPDis",    "GE",
    "Res0",    "Res1",     "CacheErr",  "Res2"
  };
  return exc_codes[(cause >> 2) & 0x1f];
}
