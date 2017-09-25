
INTERFACE:

#include "l4_types.h"

class Trap_state
{
  friend class Jdb_tcb;
  friend class Jdb_stack_view;
public:
  typedef FIASCO_FASTCALL int (*Handler)(Trap_state*, unsigned cpu);
  static Handler base_handler asm ("BASE_TRAP_HANDLER");

  // No saved segment registers

  // register state frame
  Mword  _r15;
  Mword  _r14;
  Mword  _r13;
  Mword  _r12;
  Mword  _r11;
  Mword  _r10;
  Mword  _r9;
  Mword  _r8;
  Mword  _di;
  Mword  _si;
  Mword  _bp;
  Mword  _cr2;  // we save cr2 over esp for page faults
  Mword  _bx;
  Mword  _dx;
  Mword  _cx;
  Mword  _ax;

  // Processor trap number, 0-31
  Mword  _trapno;

  // Error code pushed by the processor, 0 if none
  Mword  _err;

  // Processor state frame
  Mword  _ip;
  Mword  _cs;
  Mword  _flags;
  Mword  _sp;
  Mword  _ss;
};

struct Trex
{
  Trap_state s;
  Mword fs_base;
  Mword gs_base;
  Unsigned16 ds, es, fs, gs;

  void set_ipc_upcall()
  {
    s._err = 0;
    s._trapno = 0xfe;
  }

  void dump() { s.dump(); }
};

namespace Ts
{
  enum
  {
    /// full number of words in a Trap_state
    Words = sizeof(Trap_state) / sizeof(Mword),
    /// words for the IRET frame at the end of the trap state
    Iret_words = 5,
    /// words for error code and trap number
    Code_words = 2,
    /// offset of the IRET frame
    Iret_offset = Words - Iret_words,
    /// number of words used for normal registers
    Reg_words = Words - Iret_words - Code_words,
  };
}

IMPLEMENTATION:

#include <cstdio>
#include <panic.h>
#include "cpu.h"
#include "atomic.h"
#include "gdt.h"
#include "regdefs.h"
#include "mem.h"

Trap_state::Handler Trap_state::base_handler FIASCO_FASTCALL;

PUBLIC inline NEEDS["regdefs.h", "gdt.h"]
void
Trap_state::sanitize_user_state()
{
  if (EXPECT_FALSE(   (_cs != (Gdt::gdt_code_user | Gdt::Selector_user))
                   && (_cs != (Gdt::gdt_code_user32 | Gdt::Selector_user))))
    _cs = Gdt::gdt_code_user | Gdt::Selector_user;
  _ss = Gdt::gdt_data_user | Gdt::Selector_user;
  _flags = (_flags & ~(EFLAGS_IOPL | EFLAGS_NT)) | EFLAGS_IF;
}

PUBLIC inline NEEDS[Trap_state::sanitize_user_state, "mem.h"]
void
Trap_state::copy_and_sanitize(Trap_state const *src)
{
  Mem::memcpy_mwords(this, src, sizeof(*this) / sizeof(Mword));
  sanitize_user_state();
}

PUBLIC inline
void
Trap_state::set_pagefault(Mword pfa, Mword error)
{
  _cr2 = pfa;
  _trapno = 0xe;
  _err = error;
}

PUBLIC inline
Mword
Trap_state::trapno() const
{ return _trapno; }

PUBLIC inline
Mword
Trap_state::error() const
{ return _err; }

PUBLIC inline
Mword
Trap_state::ip() const
{ return _ip; }

PUBLIC inline
Mword
Trap_state::cs() const
{ return _cs; }

PUBLIC inline
Mword
Trap_state::flags() const
{ return _flags; }

PUBLIC inline
Mword
Trap_state::sp() const
{ return _sp; }

PUBLIC inline
Mword
Trap_state::ss() const
{ return _ss; }

PUBLIC inline
Mword
Trap_state::value() const
{ return _ax; }

PUBLIC inline
Mword
Trap_state::value2() const
{ return _cx; }

PUBLIC inline
Mword
Trap_state::value3() const
{ return _dx; }

PUBLIC inline
Mword
Trap_state::dx() const
{ return _dx; }

PUBLIC inline
Mword
Trap_state::value4() const
{ return _bx; }

PUBLIC inline
void
Trap_state::ip(Mword ip)
{ _ip = ip; }

PUBLIC inline
void
Trap_state::cs(Mword cs)
{ _cs = cs; }

PUBLIC inline
void
Trap_state::flags(Mword flags)
{ _flags = flags; }

PUBLIC inline
void
Trap_state::sp(Mword sp)
{ _sp = sp; }

PUBLIC inline
void
Trap_state::ss(Mword ss)
{ _ss = ss; }

PUBLIC inline
void
Trap_state::value(Mword value)
{ _ax = value; }

PUBLIC inline
void
Trap_state::value3(Mword value)
{ _dx = value; }

PUBLIC inline NEEDS["atomic.h"] 
void
Trap_state::consume_instruction(unsigned count)
{ cas ((Address*)(&_ip), _ip, _ip + count); }

PUBLIC inline
bool
Trap_state::is_debug_exception() const
{ return _trapno == 1 || _trapno == 3; }

PUBLIC
void
Trap_state::dump()
{
  int from_user = _cs & 3;

  printf("RAX %016lx    RBX %016lx\n", _ax, _bx);
  printf("RCX %016lx    RDX %016lx\n", _cx, _dx);
  printf("RSI %016lx    RDI %016lx\n", _si, _di);
  printf("RBP %016lx    RSP %016lx\n", _bp, from_user ? _sp : (Address)&_sp);
  printf("R8  %016lx    R9  %016lx\n", _r8,  _r9);
  printf("R10 %016lx    R11 %016lx\n", _r10, _r11);
  printf("R12 %016lx    R12 %016lx\n", _r12, _r13);
  printf("R14 %016lx    R15 %016lx\n", _r14, _r15);
  printf("RIP %016lx RFLAGS %016lx\n", _ip, _flags);
  printf("CS %04lx SS %04lx\n", _cs, _ss);
  printf("\n");
  printf("trapno %lu, error %lx, from %s mode\n",
         _trapno, _err, from_user ? "user" : "kernel");

  if (_trapno == 13)
    {
      if (_err & 1)
	printf("(external event");
      else
	printf("(internal event");
      if (_err & 2)
	{
	  printf(" regarding IDT gate descriptor no. 0x%02lx)\n", _err >> 3);
	}
      else
	{
	  printf(" regarding %s entry no. 0x%02lx)\n",
	      _err & 4 ? "LDT" : "GDT", _err >> 3);
	}
    }
  else if (_trapno == 14)
    printf("page fault linear address %16lx\n", _cr2);
}

extern "C" FIASCO_FASTCALL
void
trap_dump_panic(Trap_state *ts)
{
  ts->dump();
  panic("terminated due to trap");
}

PUBLIC inline
bool
Trap_state::exclude_logging()
{ return _trapno == 1 || _trapno == 3; }
