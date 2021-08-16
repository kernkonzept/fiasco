INTERFACE [riscv]:

#include "cpu.h"
#include "trap_state.h"

class Jdb_entry_frame : public Trap_state
{
public:
  Address_type from_user() const;
  Address ip() const;
};


//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

IMPLEMENT inline
Address_type
Jdb_entry_frame::from_user() const
{ return user_mode() ? ADDR_USER : ADDR_KERNEL; }

IMPLEMENT inline
Address
Jdb_entry_frame::ip() const
{ return _pc; }

PUBLIC inline
Address
Jdb_entry_frame::ksp() const
{ return Address(this); }

PUBLIC inline
Mword
Jdb_entry_frame::param() const
{ return a1; }

PUBLIC inline
char const *
Jdb_entry_frame::text() const
{ return reinterpret_cast<char const *>(a1); }

PUBLIC inline
unsigned
Jdb_entry_frame::textlen() const
{ return a2; }

PRIVATE inline
bool
Jdb_entry_frame::debug_entry_kernel() const
{ return !user_mode() && cause == Cpu::Exc_breakpoint; }

PUBLIC inline NEEDS[Jdb_entry_frame::debug_entry_kernel]
bool
Jdb_entry_frame::debug_entry_kernel_str() const
{ return debug_entry_kernel() && a0 == 0; }

PUBLIC inline NEEDS[Jdb_entry_frame::debug_entry_kernel]
bool
Jdb_entry_frame::debug_entry_user_str() const
{ return debug_entry_kernel() && a0 == 1; }

PUBLIC inline NEEDS[Jdb_entry_frame::debug_entry_kernel]
bool
Jdb_entry_frame::debug_entry_kernel_sequence() const
{ return debug_entry_kernel() && a0 == 2; }

PUBLIC inline
bool
Jdb_entry_frame::debug_ipi() const
{ return cause == Trap_state::Ec_l4_debug_ipi; }
