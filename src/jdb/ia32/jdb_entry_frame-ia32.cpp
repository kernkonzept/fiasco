INTERFACE:

#include "cpu.h"
#include "tb_entry.h"
#include "trap_state.h"
#include "tb_entry.h"

class Jdb_entry_frame : public Trap_state
{};

//---------------------------------------------------------------------------
IMPLEMENTATION[ia32,ux,amd64]:

PUBLIC inline
bool
Jdb_entry_frame::debug_entry_kernel_str() const
{ return _trapno == 3 && _ax == 0; }

PUBLIC inline
bool
Jdb_entry_frame::debug_entry_user_str() const
{ return _trapno == 3 && _ax == 1; }

PUBLIC inline
bool
Jdb_entry_frame::debug_entry_kernel_sequence() const
{ return _trapno == 3 && _ax == 2; }

PUBLIC inline
bool
Jdb_entry_frame::debug_ipi() const
{ return _trapno == 0xee; }

PUBLIC inline
Address_type
Jdb_entry_frame::from_user() const
{ return cs() & 3 ? ADDR_USER : ADDR_KERNEL; }

PUBLIC inline
Address
Jdb_entry_frame::ksp() const
{ return (Address)&_sp; }

PUBLIC inline
Address
Jdb_entry_frame::sp() const
{ return from_user() ? _sp : ksp(); }

PUBLIC inline
Mword
Jdb_entry_frame::param() const
{ return _ax; }

PUBLIC inline
char const *
Jdb_entry_frame::text() const
{ return reinterpret_cast<char const *>(_cx); }

PUBLIC inline
unsigned
Jdb_entry_frame::textlen() const
{ return _dx; }


//---------------------------------------------------------------------------
IMPLEMENTATION[ia32,ux]:

PUBLIC inline
Mword
Jdb_entry_frame::get_reg(unsigned reg) const
{
  Mword val = 0;

  switch (reg)
    {
    case 1:  val = _ax; break;
    case 2:  val = _bx; break;
    case 3:  val = _cx; break;
    case 4:  val = _dx; break;
    case 5:  val = _bp; break;
    case 6:  val = _si; break;
    case 7:  val = _di; break;
    case 8:  val = _ip; break;
    case 9:  val = _sp; break;
    case 10: val = _flags; break;
    }

  return val;
}


//---------------------------------------------------------------------------
IMPLEMENTATION[amd64]:

PUBLIC inline
Mword
Jdb_entry_frame::get_reg(unsigned reg) const
{
  Mword val = 0;

  switch (reg)
    {
    case 1:  val = _ax; break;
    case 2:  val = _bx; break;
    case 3:  val = _cx; break;
    case 4:  val = _dx; break;
    case 5:  val = _bp; break;
    case 6:  val = _si; break;
    case 7:  val = _di; break;
    case 8:  val = _r8;  break;
    case 9:  val = _r9;  break;
    case 10: val = _r10; break;
    case 11: val = _r11; break;
    case 12: val = _r12; break;
    case 13: val = _r13; break;
    case 14: val = _r14; break;
    case 15: val = _r15; break;
    case 16: val = _ip; break;
    case 17: val = _sp; break;
    case 18: val = _flags; break;
    }

  return val;
}

//---------------------------------------------------------------------------
IMPLEMENTATION[ia32,amd64]:

PUBLIC inline NEEDS["cpu.h"]
Mword
Jdb_entry_frame::ss() const
{ return from_user() ? _ss : Cpu::get_ss(); }

//---------------------------------------------------------------------------
IMPLEMENTATION[ux]:

PUBLIC
Mword
Jdb_entry_frame::ss() const
{ return _ss; }
