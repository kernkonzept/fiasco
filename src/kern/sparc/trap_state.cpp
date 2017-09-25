
INTERFACE:

#include "l4_types.h"
#include "entry_frame.h"

class Trap_state_regs
{
public:
  Mword pf_address;
  Mword error_code;
};


class Trap_state : public Trap_state_regs, public Syscall_frame,
                   public Return_frame
{
public:
  typedef int (*Handler)(Trap_state*, Cpu_number cpu);
  bool exclude_logging() { return false; }
};

struct Trex
{
  Trap_state s;
  void set_ipc_upcall()
  { s.error_code = 0x10000000; /* see Msr */ }

  void dump() { s.dump(); }
};


IMPLEMENTATION:

#include <cstdio>

PUBLIC inline
void
Trap_state::copy_and_sanitize(Trap_state const *)
{
  // FIXME: unimplemented
}

PUBLIC inline
unsigned long
Trap_state::ip() const
{ return srr0; }

PUBLIC inline
unsigned long
Trap_state::trapno() const
{ return error_code; }

PUBLIC inline
Mword
Trap_state::error() const
{ return 0; }

PUBLIC inline
void
Trap_state::set_pagefault(Mword pfa, Mword error)
{
  pf_address = pfa;
  error_code = error;
}

PUBLIC inline
bool
Trap_state::is_debug_exception() const
{ return false; }

PUBLIC
void
Trap_state::dump()
{
  char const *excpts[] = 
    {"reset","machine check"};
  
  printf("EXCEPTION: pfa=%08lx, error=%08lx\n",
         //excpts[((error_code & ~0xff) >> 8) - 1]
          pf_address, error_code);

  printf("SP: %08lx LR: %08lx SRR0: %08lx SRR1 %08lx\n\n"
         "R[0]  %08lx\n"
	 "R[3]: %08lx %08lx %08lx %08lx %08lx\n"
         "R[8]: %08lx %08lx %08lx %08lx %08lx\n",
         usp, ulr, srr0, srr1,
	 r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
	 r[8], r11, r12);
}

