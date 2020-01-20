INTERFACE [ppc32]:

#include "msr.h"
#include "trap_state.h"
#include "tb_entry.h"

class Jdb_entry_frame : public Trap_state
{
public:
  Address_type from_user() const;
  Address ip() const;
  bool debug_ipi() const;
};

//---------------------------------------------------------------------------
IMPLEMENTATION[ppc32]:

#include <cstdio>
#include "processor.h"

#if 0
PUBLIC
void
Jdb_entry_frame::dump() const
{
  printf(
      "R[ 0- 3]: %08lx %08lx %08lx %08lx\n"
      "R[ 4- 7]: %08lx %08lx %08lx %08lx\n"
      "R[ 8-11]: %08lx %08lx %08lx %08lx\n"
      "R[12-15]: %08lx %08lx %08lx %08lx\n"
      "kernel sp = %08lx  cpsr = %08lx  spsr = %08lx\n",
      r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8],
      r[9], r[10], r[11], r[12], r[13], r[14], pc, ksp, cpsr, spsr);
}
#endif

IMPLEMENT inline
bool
Jdb_entry_frame::debug_ipi() const
{ return false; }

IMPLEMENT inline NEEDS["msr.h"]
Address_type
Jdb_entry_frame::from_user() const
{
  return (srr1 & Msr::Msr_pr) ? ADDR_USER : ADDR_KERNEL;
}

PUBLIC inline
Address Jdb_entry_frame::ksp() const
{ return Address(this); }

IMPLEMENT inline
Address Jdb_entry_frame::ip() const
{ return srr0; }

PUBLIC inline
Mword
Jdb_entry_frame::param() const
{ return r[2]; /*r3*/ }

PUBLIC inline
char const *
Jdb_entry_frame::text() const
{ return reinterpret_cast<char const *>(r[2]); }

PUBLIC inline
unsigned
Jdb_entry_frame::textlen() const
{ return r[3]; }
