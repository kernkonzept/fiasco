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
  return c.exc_code() == 9 && r[Entry_frame::R_t0] == 1;
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

