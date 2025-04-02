
INTERFACE:

#include "l4_types.h"
#include "entry_frame.h"
#include "processor.h"

class Trap_state_regs
{
public:
//  static int (*base_handler)(Trap_state *) asm ("BASE_TRAP_HANDLER");

  Mword pf_address;
  union
  {
    Mword error_code;
    Arm_esr esr;
  };

  Mword r[13];
};

class Trap_state : public Trap_state_regs, public Return_frame
{
public:
  typedef int (*Handler)(Trap_state*, Cpu_number cpu);
  bool exclude_logging() { return false; }
};

struct Trex
{
  Trap_state s;
  Mword tpidruro;
  Mword tpidrurw;
  void set_ipc_upcall()
  { s.esr.ec() = 0x3f; }

  void dump() { s.dump(); }
};


//-----------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

#include "processor.h"
#include "mem.h"

PUBLIC inline NEEDS["processor.h", "mem.h"]
void
Trap_state::copy_and_sanitize(Trap_state const *src)
{
  // copy pf_address, esr, r0..r12, usp, ulr / omit km_lr
  Mem::memcpy_mwords(this, src, 17);
  pc = src->pc;
  psr = access_once(&src->psr);
  psr &= ~(Mword{Proc::Status_mode_mask} | Mword{Proc::Status_interrupts_mask});
  psr |= Mword{Proc::Status_mode_user} | Mword{Proc::Status_always_mask};
}

//-----------------------------------------------------------------
IMPLEMENTATION:

#include "mem_layout.h"

#include <cstdio>

PUBLIC inline
void
Trap_state::set_pagefault(Mword pfa, Mword error)
{
  pf_address = pfa;
  error_code = error;
}

PUBLIC inline
unsigned long
Trap_state::trapno() const
{ return esr.ec(); }

PUBLIC inline
Mword
Trap_state::error() const
{ return error_code; }

PUBLIC inline
bool
Trap_state::exception_is_undef_insn() const
{ return esr.ec() == 0; }

PUBLIC
void
Trap_state::dump()
{
  static constinit char const *const excpts[] =
  {
    /* 0 */ "undef insn", "WFx", nullptr, "MCR (CP15)",
    /* 4 */ "MCRR (CP15)", "MCR (CP14)", "LDC (CP14)", "coproc trap",
    /* 8 */ "MRC (CP10)", nullptr, "BXJ", nullptr,
    /* C */ "MRRC (CP14)", nullptr, nullptr, nullptr,
    /* 10 */ nullptr, "SVC", "HVC", "SMC",
    /* 14 */ nullptr, nullptr, nullptr, nullptr,
    /* 18 */ nullptr, nullptr, nullptr, nullptr,
    /* 1C */ nullptr, nullptr, nullptr, nullptr,
    /* 20 */ "prefetch abt (usr)", "prefetch abt (kernel)", nullptr, nullptr,
    /* 24 */ "data abt (user)", "data abt (kernel)", nullptr, nullptr,
    /* 28 */ nullptr, nullptr, nullptr, nullptr,
    /* 2C */ nullptr, nullptr, nullptr, nullptr,
    /* 30 */ nullptr, nullptr, nullptr, nullptr,
    /* 34 */ nullptr, nullptr, nullptr, nullptr,
    /* 38 */ nullptr, nullptr, nullptr, nullptr,
    /* 3C */ nullptr, nullptr, "<TrExc>", "<IPC>"
  };

  printf("EXCEPTION: (%02x) %s pfa=%08lx, error=%08lx psr=%08lx\n",
         esr.ec().get(), excpts[esr.ec()] ? excpts[esr.ec()] : "", pf_address,
         error_code, psr);

  printf("R[0]: %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         "R[8]: %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n",
	 r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
	 r[8], r[9], r[10], r[11], r[12], usp, ulr, pc);

  extern char virt_address[] asm ("virt_address");
  Mword lower_limit = reinterpret_cast<Mword>(&virt_address);
  Mword upper_limit = reinterpret_cast<Mword>(&Mem_layout::initcall_end);
  if (lower_limit <= pc && pc < upper_limit)
    {
      printf("Data around PC at 0x%lx:\n", pc);
      for (Mword d = pc - 24; d < pc + 28; d += 4)
        if (lower_limit <= d && d < upper_limit)
          printf("%s0x%08lx: %08x\n", d == pc ? "->" : "  ", d,
                                      *reinterpret_cast<unsigned *>(d));
    }
}
