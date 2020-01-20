INTERFACE [arm]:

#include "cpu.h"
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
IMPLEMENTATION[arm]:

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

IMPLEMENT inline NEEDS["processor.h"]
Address_type
Jdb_entry_frame::from_user() const
{ return check_valid_user_psr() ? ADDR_USER : ADDR_KERNEL; }

PUBLIC inline
Address Jdb_entry_frame::ksp() const
{ return Address(this); }

IMPLEMENT inline
Address Jdb_entry_frame::ip() const
{ return pc; }

PUBLIC inline
Mword
Jdb_entry_frame::param() const
{ return r[0]; }

PUBLIC inline
char const *
Jdb_entry_frame::text() const
{ return reinterpret_cast<char const *>(r[0]); }

PUBLIC inline
unsigned
Jdb_entry_frame::textlen() const
{ return r[1]; }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit]:

// Error = 0x33UL: see DEBUGGER_ENTRY

PUBLIC inline
bool
Jdb_entry_frame::debug_entry_kernel_str() const
{ return error_code == (0x33UL << 26); }

PUBLIC inline
bool
Jdb_entry_frame::debug_entry_user_str() const
{ return error_code == ((0x33UL << 26) | 1); }

PUBLIC inline
bool
Jdb_entry_frame::debug_entry_kernel_sequence() const
{ return error_code == ((0x33UL << 26) | 2); }

IMPLEMENT inline
bool
Jdb_entry_frame::debug_ipi() const
{ return error_code == ((0x33UL << 26) | 3); }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit]:

// Error 0x3cUL: 'brk'

PUBLIC inline
bool
Jdb_entry_frame::debug_entry_kernel_str() const
{ return error_code == ((0x3cUL << 26) | (1 << 25)); }

PUBLIC inline
bool
Jdb_entry_frame::debug_entry_user_str() const
{ return error_code == ((0x3cUL << 26) | (1 << 25) | 1); }

PUBLIC inline
bool
Jdb_entry_frame::debug_entry_kernel_sequence() const
{ return error_code == ((0x3cUL << 26) | (1 << 25) | 2); }

IMPLEMENT inline
bool
Jdb_entry_frame::debug_ipi() const
{ return error_code == ((0x3cUL << 26) | (1 << 25) | 3); }

