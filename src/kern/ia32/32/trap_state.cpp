
INTERFACE:

#include "l4_types.h"

class Trap_state
{
  friend class Jdb_tcb;
  friend class Jdb_stack_view;

public:
  typedef FIASCO_FASTCALL int (*Handler)(Trap_state*, unsigned cpu);
  static Handler base_handler asm ("BASE_TRAP_HANDLER");

  // Saved segment registers
  Mword  _es;
  Mword  _ds;
  Mword  _gs;                                     // => utcb->values[ 0]
  Mword  _fs;                                     // => utcb->values[ 1]

  // PUSHA register state frame
  Mword  _di;                                     // => utcb->values[ 2]
  Mword  _si;                                     // => utcb->values[ 3]
  Mword  _bp;                                     // => utcb->values[ 4]
  Mword  _cr2; // we save cr2 over esp for PFs    // => utcb->values[ 5]
  Mword  _bx;                                     // => utcb->values[ 6]
  Mword  _dx;                                     // => utcb->values[ 7]
  Mword  _cx;                                     // => utcb->values[ 8]
  Mword  _ax;                                     // => utcb->values[ 9]

  // Processor trap number, 0-31
  Mword  _trapno;                                 // => utcb->values[10]

  // Error code pushed by the processor, 0 if none
  Mword  _err;                                    // => utcb->values[11]

  // Processor state frame
  Mword  _ip;                                     // => utcb->values[12]
  Mword  _cs;                                     // => utcb->values[13]
  Mword  _flags;                                  // => utcb->values[14]
  Mword  _sp;                                     // => utcb->values[15]
  Mword  _ss;
};

struct Trex
{
  Trap_state s;

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

//---------------------------------------------------------------------------
IMPLEMENTATION [ia32]:

#include "regdefs.h"
#include "gdt.h"

PUBLIC inline NEEDS["regdefs.h", "gdt.h"]
void
Trap_state::sanitize_user_state()
{
  _cs = Gdt::gdt_code_user | Gdt::Selector_user;
  _ss = Gdt::gdt_data_user | Gdt::Selector_user;
  _flags = (_flags & ~(EFLAGS_IOPL | EFLAGS_NT)) | EFLAGS_IF;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [ux]:

#include "emulation.h"
#include "regdefs.h"

PUBLIC inline NEEDS["emulation.h", "regdefs.h"]
void
Trap_state::sanitize_user_state()
{
  _cs = Emulation::kernel_cs() & ~1;
  _ss = Emulation::kernel_ss();
  _flags = (_flags & ~(EFLAGS_IOPL | EFLAGS_NT)) | EFLAGS_IF;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [ia32 || ux]:

#include <cstdio>
#include <panic.h>
#include "cpu.h"
#include "atomic.h"
#include "mem.h"

Trap_state::Handler Trap_state::base_handler FIASCO_FASTCALL;

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
Trap_state::dx() const
{ return _dx; }

PUBLIC inline
Mword
Trap_state::value3() const
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

  printf("EAX %08lx EBX %08lx ECX %08lx EDX %08lx\n"
         "ESI %08lx EDI %08lx EBP %08lx ESP %08lx\n"
         "EIP %08lx EFLAGS %08lx\n"
         "CS %04lx SS %04lx DS %04lx ES %04lx FS %04lx GS %04lx\n"
         "trap %lu (%s), error %08lx, from %s mode\n",
	 _ax, _bx, _cx, _dx,
	 _si, _di, _bp, from_user ? _sp : (Unsigned32)&_sp,
	 _ip, _flags,
	 _cs & 0xffff, from_user ? _ss & 0xffff : Cpu::get_ss() & 0xffff,
	 _ds & 0xffff, _es & 0xffff, _fs & 0xffff, _gs & 0xffff,
	 _trapno, Cpu::exception_string(_trapno), _err, from_user ? "user" : "kernel");

  if (_trapno == 13)
    {
      if (_err & 1)
	printf("(external event");
      else
	printf("(internal event");
      if (_err & 2)
	printf(" regarding IDT gate descriptor no. 0x%02lx)\n", _err >> 3);
      else
	printf(" regarding %s entry no. 0x%02lx)\n",
	      _err & 4 ? "LDT" : "GDT", _err >> 3);
    }
  else if (_trapno == 14)
    printf("page fault linear address %08lx\n", _cr2);
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
