
INTERFACE:

#include "l4_types.h"
#include "entry_frame.h"
#include <cxx/bitfield>

class Ts_error_code
{
public:
  Ts_error_code() = default;
  explicit Ts_error_code(Mword ec) : _raw(ec) {}
  Mword _raw;

  Mword raw() const { return _raw; }

  CXX_BITFIELD_MEMBER(26, 31, ec, _raw);
  CXX_BITFIELD_MEMBER(25, 25, il, _raw);
  CXX_BITFIELD_MEMBER(24, 24, cv, _raw);
  CXX_BITFIELD_MEMBER(20, 23, cond, _raw);

  /** \pre ec == 0x01 */
  CXX_BITFIELD_MEMBER( 0,  0, wfe_trapped, _raw);

  CXX_BITFIELD_MEMBER(17, 19, mcr_opc2, _raw);
  CXX_BITFIELD_MEMBER(16, 19, mcrr_opc1, _raw);
  CXX_BITFIELD_MEMBER(14, 16, mcr_opc1, _raw);
  CXX_BITFIELD_MEMBER(10, 13, mcr_crn, _raw);
  CXX_BITFIELD_MEMBER(10, 13, mcrr_rt2, _raw);
  CXX_BITFIELD_MEMBER( 5,  8, mcr_rt, _raw);
  CXX_BITFIELD_MEMBER( 1,  4, mcr_crm, _raw);
  CXX_BITFIELD_MEMBER( 0,  0, mcr_read, _raw);

  Mword mcr_coproc_register() const { return _raw & 0xffc1f; }

  static Mword mcr_coproc_register(unsigned opc1, unsigned crn, unsigned crm, unsigned opc2)
  { return   mcr_opc1_bfm_t::val_dirty(opc1)
           | mcr_crn_bfm_t::val_dirty(crn)
           | mcr_crm_bfm_t::val_dirty(crm)
           | mcr_opc2_bfm_t::val_dirty(opc2); }

  static Mword mrc_coproc_register(unsigned opc1, unsigned crn, unsigned crm, unsigned opc2)
  { return mcr_coproc_register(opc1, crn, crm, opc2) | 1; }

  CXX_BITFIELD_MEMBER(12, 19, ldc_imm, _raw);
  CXX_BITFIELD_MEMBER( 5,  8, ldc_rn, _raw);
  CXX_BITFIELD_MEMBER( 4,  4, ldc_offset_form, _raw);
  CXX_BITFIELD_MEMBER( 1,  3, ldc_addressing_mode, _raw);

  CXX_BITFIELD_MEMBER( 5,  5, cpt_simd, _raw);
  CXX_BITFIELD_MEMBER( 0,  3, cpt_cpnr, _raw);

  CXX_BITFIELD_MEMBER( 0,  3, bxj_rm, _raw);

  CXX_BITFIELD_MEMBER( 0, 15, svc_imm, _raw);

  CXX_BITFIELD_MEMBER(24, 24, pf_isv, _raw);
  CXX_BITFIELD_MEMBER(22, 23, pf_sas, _raw);
  CXX_BITFIELD_MEMBER(21, 21, pf_sse, _raw);
  CXX_BITFIELD_MEMBER(16, 19, pf_srt, _raw);
  CXX_BITFIELD_MEMBER( 9,  9, pf_ea, _raw);
  CXX_BITFIELD_MEMBER( 8,  8, pf_cache_maint, _raw);
  CXX_BITFIELD_MEMBER( 7,  7, pf_s1ptw, _raw);
  CXX_BITFIELD_MEMBER( 6,  6, pf_write, _raw);
  CXX_BITFIELD_MEMBER( 0,  5, pf_fsc, _raw);
};

class Trap_state_regs
{
public:
//  static int (*base_handler)(Trap_state *) asm ("BASE_TRAP_HANDLER");

  Mword pf_address;
  union
  {
    Mword error_code;
    Ts_error_code esr;
  };

  Mword tpidruro;
  Mword r[13];
};

class Trap_state : public Trap_state_regs, public Return_frame
{
public:
  typedef int (*Handler)(Trap_state*, unsigned cpu);
  bool exclude_logging() { return false; }
};

struct Trex
{
  Trap_state s;
  void set_ipc_upcall()
  { s.esr.ec() = 0x3f; }

  void dump() { s.dump(); }
};


//-----------------------------------------------------------------
IMPLEMENTATION [arm && !hyp]:

#include "processor.h"
#include "mem.h"

PUBLIC inline NEEDS["processor.h", "mem.h"]
void
Trap_state::copy_and_sanitize(Trap_state const *src)
{
  // copy up to and including ulr
  Mem::memcpy_mwords(this, src, 19);
  // skip km_lr
  pc = src->pc;
  psr = access_once(&src->psr);
  psr &= ~(Proc::Status_mode_mask | Proc::Status_interrupts_mask);
  psr |= Proc::Status_mode_user | Proc::Status_always_mask;
}

//-----------------------------------------------------------------
IMPLEMENTATION:

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
Trap_state::ip() const
{ return pc; }

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

PUBLIC inline
bool
Trap_state::is_debug_exception() const
{ return esr.ec() == 0x24 && esr.pf_fsc() == 0x22; }

PUBLIC
void
Trap_state::dump()
{
  char const *excpts[] =
    {/*  0 */ "undef insn",  "WFx",        0,            "MCR (CP15)",
     /*  4 */ "MCRR (CP15)", "MCR (CP14)", "LDC (CP14)", "coproc trap",
     /*  8 */ "MRC (CP10)",  0,            "BXJ",        0,
     /*  C */ "MRRC (CP14)", 0,            0,            0,
     /* 10 */ 0,             "SVC",        "HVC",        "SMC",
     /* 14 */ 0, 0, 0, 0,
     /* 18 */ 0, 0, 0, 0,
     /* 1C */ 0, 0, 0, 0,
     /* 20 */ "prefetch abt (usr)", "prefetch abt (kernel)", 0, 0,
     /* 24 */ "data abt (user)",    "data abt (kernel)",     0, 0,
     /* 28 */ 0, 0, 0, 0,
     /* 2C */ 0, 0, 0, 0,
     /* 30 */ 0, 0, 0, 0,
     /* 34 */ 0, 0, 0, 0,
     /* 38 */ 0, 0, 0, 0,
     /* 3C */ 0, 0, "<TrExc>", "<IPC>"};

  printf("EXCEPTION: (%02x) %s pfa=%08lx, error=%08lx psr=%08lx\n",
         (unsigned)esr.ec(), excpts[esr.ec()] ? excpts[esr.ec()] : "",
         pf_address, error_code, psr);

  printf("R[0]: %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         "R[8]: %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n",
	 r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
	 r[8], r[9], r[10], r[11], r[12], usp, ulr, pc);
}
