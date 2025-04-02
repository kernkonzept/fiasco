INTERFACE:

#include "entry_frame.h"

class Trap_state : public Entry_frame
{
public:
  typedef int (*Handler)(Trap_state*, Cpu_number cpu);

  // generally MIPS encodes the error code and trapno into the cause
  // register, so we return status ess error code however
  Mword trapno() const { return cause; }
  Mword error() const { return status; }

  void set_pagefault(Mword pfa, Mword cause)
  {
    this->bad_v_addr = pfa;
    this->cause = cause;
  }
};

struct Trex
{
  Trap_state s;
  Mword ulr;

  void set_ipc_upcall()
  { s.cause = Trap_state::C_l4_ipc_upcall; }

  void dump() { s.dump(); }
};

IMPLEMENTATION:


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

