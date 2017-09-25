INTERFACE:

#include "cpu.h"
#include "tb_entry.h"
#include "trap_state.h"
#include "tb_entry.h"

class Jdb_entry_frame : public Trap_state
{};

class Jdb_output_frame : public Jdb_entry_frame
{};

class Jdb_status_page_frame : public Jdb_entry_frame
{};

class Jdb_log_frame : public Jdb_entry_frame
{};

class Jdb_log_3val_frame : public Jdb_log_frame
{};

class Jdb_debug_frame : public Jdb_entry_frame
{};

class Jdb_symbols_frame : public Jdb_debug_frame
{};

class Jdb_lines_frame : public Jdb_debug_frame
{};

//---------------------------------------------------------------------------
IMPLEMENTATION[ia32,ux,amd64]:

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

//---------------------------------------------------------------------------
IMPLEMENTATION[ia32,ux,amd64]:

PUBLIC inline
Unsigned8*
Jdb_output_frame::str() const
{ return (Unsigned8*)_ax; }

PUBLIC inline
int
Jdb_output_frame::len() const
{ return (unsigned)_bx; }

//---------------------------------------------------------------------------
PUBLIC inline
void
Jdb_status_page_frame::set(Address status_page)
{ _ax = (Mword)status_page; }

//---------------------------------------------------------------------------
PUBLIC inline
Unsigned8*
Jdb_log_frame::str() const
{ return (Unsigned8*)_dx; }

PUBLIC inline NEEDS["tb_entry.h"]
void
Jdb_log_frame::set_tb_entry(Tb_entry* tb_entry)
{ _ax = (Mword)tb_entry; }

//---------------------------------------------------------------------------
PUBLIC inline
Mword
Jdb_log_3val_frame::val1() const
{ return _cx; }

PUBLIC inline
Mword
Jdb_log_3val_frame::val2() const
{ return _si; }

PUBLIC inline
Mword
Jdb_log_3val_frame::val3() const
{ return _di; }

//---------------------------------------------------------------------------
PUBLIC inline
Mword
Jdb_debug_frame::addr() const
{ return _ax; }

PUBLIC inline
Mword
Jdb_debug_frame::size() const
{ return _dx; }

