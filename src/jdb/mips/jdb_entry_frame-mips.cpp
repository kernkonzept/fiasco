INTERFACE [mips]:

#include "cp0_status.h"
#include "trap_state.h"
#include "tb_entry.h"

class Jdb_entry_frame : public Trap_state
{
public:
  Address_type from_user() const;
  Address ip() const;
  bool debug_trap() const;
  bool debug_sequence() const;
  bool debug_ipi() const;
};

class Jdb_output_frame : public Jdb_entry_frame {};
class Jdb_status_page_frame : public Jdb_entry_frame {};
class Jdb_log_frame : public Jdb_entry_frame {};
class Jdb_log_3val_frame : public Jdb_log_frame {};
class Jdb_debug_frame : public Jdb_entry_frame {};
class Jdb_symbols_frame : public Jdb_debug_frame {};
class Jdb_lines_frame : public Jdb_debug_frame {};
class Jdb_get_cputime_frame : public Jdb_entry_frame {};
class Jdb_thread_name_frame : public Jdb_entry_frame {};

//---------------------------------------------------------------------------
IMPLEMENTATION[mips]:

#include <cstdio>
#include "processor.h"

IMPLEMENT inline
bool
Jdb_entry_frame::debug_trap() const
{
  Cause c(cause);
  return c.exc_code() == 9;
}

IMPLEMENT inline
bool
Jdb_entry_frame::debug_sequence() const
{
  Cause c(cause);
  return c.exc_code() == 9 && c.bp_spec() == 1;
}

IMPLEMENT inline
bool
Jdb_entry_frame::debug_ipi() const
{
  Cause c(cause);
  return c.exc_code() == 9 && c.bp_spec() == 2;
}

IMPLEMENT inline NEEDS["cp0_status.h"]
Address_type
Jdb_entry_frame::from_user() const
{
  return (status & Cp0_status::ST_KSU_USER) ? ADDR_USER : ADDR_KERNEL;
}

PUBLIC inline
Address Jdb_entry_frame::ksp() const
{ return Address(this); }

IMPLEMENT inline
Address Jdb_entry_frame::ip() const
{ return epc; }

//---------------------------------------------------------------------------
// the following register usage matches ABI in l4sys/include/ARCH-mips/kdebug.h

PUBLIC inline
Mword
Jdb_entry_frame::param() const
{ return r[Entry_frame::R_t1]; }

PUBLIC inline
Unsigned8*
Jdb_log_frame::str() const
{ return (Unsigned8*)r[Entry_frame::R_t2]; }

PUBLIC inline NEEDS["tb_entry.h"]
void
Jdb_log_frame::set_tb_entry(Tb_entry* tb_entry)
{ r[Entry_frame::R_t1] = (Mword)tb_entry; }

PUBLIC inline
Mword
Jdb_log_3val_frame::val1() const
{ return r[Entry_frame::R_t3]; }

PUBLIC inline
Mword
Jdb_log_3val_frame::val2() const
{ return r[Entry_frame::R_t4]; }

PUBLIC inline
Mword
Jdb_log_3val_frame::val3() const
{ return r[Entry_frame::R_t5]; }

PUBLIC inline
void
Jdb_status_page_frame::set(Address status_page)
{ r[Entry_frame::R_t1] = (Mword)status_page; }
