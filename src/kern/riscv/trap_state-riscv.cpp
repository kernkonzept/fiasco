INTERFACE:

#include "entry_frame.h"

EXTENSION class Trap_state : public Entry_frame
{};

EXTENSION struct Trex
{};

static_assert(sizeof(Trex) / sizeof(Mword) == L4_exception_ipc::Msg_size,
              "Wrong exception IPC message size.");

//----------------------------------------------------------------------------
IMPLEMENTATION:

IMPLEMENT inline
void
Trex::set_ipc_upcall()
{ s.cause = Trap_state::Ec_l4_ipc_upcall; }


IMPLEMENT inline
Mword
Trap_state::trapno() const
{ return 0; }

IMPLEMENT inline
Mword
Trap_state::error() const
{ return 0; }

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
Trap_state::set_pagefault(Mword pfa, Mword error)
{
  tval = pfa;
  cause = error;
}

IMPLEMENT inline
bool
Trap_state::exclude_logging() const
{ return false; }

IMPLEMENT inline
void
Trap_state::dump() const
{ Entry_frame::dump(); }
