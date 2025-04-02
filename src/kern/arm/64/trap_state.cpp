INTERFACE:

#include "entry_frame.h"

class Trap_state : public Entry_frame
{
public:
  typedef int (*Handler)(Trap_state*, Cpu_number cpu);

  Mword trapno() const { return esr.ec(); }
  Mword error() const { return esr.raw(); }

  void set_pagefault(Mword pfa, Mword esr)
  {
    this->pf_address = pfa;
    this->esr = Arm_esr(esr);
  }

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

IMPLEMENTATION:

#include "mem_layout.h"

#include <cstdio>

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

  printf("EXCEPTION: (%02x) %s pfa=%08lx, error=%08lx pstate=%08lx\n",
         esr.ec().get(), excpts[esr.ec()] ? excpts[esr.ec()] : "",
         pf_address, esr.raw(), pstate);

  printf("R[ 0]: %016lx %016lx %016lx %016lx\n"
         "R[ 4]: %016lx %016lx %016lx %016lx\n"
         "R[ 8]: %016lx %016lx %016lx %016lx\n"
         "R[12]: %016lx %016lx %016lx %016lx\n"
         "R[16]: %016lx %016lx %016lx %016lx\n"
         "R[20]: %016lx %016lx %016lx %016lx\n"
         "R[24]: %016lx %016lx %016lx %016lx\n"
         "R[28]: %016lx %016lx %016lx\n"
         "SP: %016lx  PC: %016lx\n",
         r[0], r[1], r[2], r[3],
         r[4], r[5], r[6], r[7],
         r[8], r[9], r[10], r[11],
         r[12], r[13], r[14], r[15],
         r[16], r[17], r[18], r[19],
         r[20], r[21], r[22], r[23],
         r[24], r[25], r[26], r[27],
         r[28], r[29], r[30],
         usp, pc);

  extern char virt_address[] asm ("virt_address");
  Mword lower_limit = reinterpret_cast<Mword>(&virt_address);
  Mword upper_limit = reinterpret_cast<Mword>(&Mem_layout::initcall_end);
  if (lower_limit <= pc && pc < upper_limit)
    {
      printf("Data around PC at 0x%lx:\n", pc);
      for (Mword d = pc - 24; d < pc + 28; d += 4)
        if (lower_limit <= d && d < upper_limit)
          printf("%s0x%012lx: %08x\n", d == pc ? "->" : "  ", d,
                                       *reinterpret_cast<unsigned *>(d));
    }
}
