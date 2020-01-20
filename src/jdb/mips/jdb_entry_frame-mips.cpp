INTERFACE [mips]:

#include "cp0_status.h"
#include "trap_state.h"
#include "tb_entry.h"

class Jdb_entry_frame : public Trap_state
{
public:
  Address_type from_user() const;
  Address ip() const;
};

//---------------------------------------------------------------------------
IMPLEMENTATION[mips]:

#include <cstdio>
#include "processor.h"

PUBLIC inline
bool
Jdb_entry_frame::debug_entry_kernel_str() const
{
  Cause c(cause);
  return c.exc_code() == 9 && r[Entry_frame::R_t0] == 0;
}

PUBLIC inline
bool
Jdb_entry_frame::debug_entry_user_str() const
{
  Cause c(cause);
  return c.exc_code() == 9 && r[Entry_frame::R_t0] == 1;
}

PUBLIC inline
bool
Jdb_entry_frame::debug_entry_kernel_sequence() const
{
  Cause c(cause);
  return c.exc_code() == 9 && r[Entry_frame::R_t0] == 2;
}

PUBLIC inline
bool
Jdb_entry_frame::debug_ipi() const
{
  Cause c(cause);
  return c.exc_code() == 9 && c.bp_spec() == 3;
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

PUBLIC inline
char const *
Jdb_entry_frame::text() const
{ return reinterpret_cast<char const *>(r[Entry_frame::R_a0]); }

PUBLIC inline
unsigned
Jdb_entry_frame::textlen() const
{ return r[Entry_frame::R_a1]; }

//---------------------------------------------------------------------------
// the following register usage matches ABI in l4sys/include/ARCH-mips/kdebug.h

PUBLIC inline
Mword
Jdb_entry_frame::param() const
{ return r[Entry_frame::R_t1]; }

