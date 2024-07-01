INTERFACE:

#include "entry_frame.h"

#include "warn.h"

class Trap_state : public Entry_frame
{
public:
  typedef int (*Handler)(Trap_state*, Cpu_number cpu);
};

struct Trex
{
  Trap_state s;

  void set_ipc_upcall()
  {
    s.cause = Trap_state::Ec_l4_ipc_upcall;
  }

  void dump()
  {
    s.dump();
  }
};

static_assert(sizeof(Trex) / sizeof(Mword) == L4_exception_ipc::Msg_size,
              "Wrong exception IPC message size.");

//----------------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC inline
unsigned long
Trap_state::trapno() const
{ return 0; }

PUBLIC inline
Mword
Trap_state::error() const
{ return 0; }

PUBLIC inline
void
Trap_state::set_pagefault(Mword pfa, Mword error)
{
  tval = pfa;
  cause = error;
}

PUBLIC inline
bool
Trap_state::exclude_logging()
{ return false; }
