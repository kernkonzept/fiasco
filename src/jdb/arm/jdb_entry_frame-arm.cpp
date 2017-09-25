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

class Jdb_get_cputime_frame : public Jdb_entry_frame
{};

class Jdb_thread_name_frame : public Jdb_entry_frame
{};

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

IMPLEMENT inline
bool
Jdb_entry_frame::debug_ipi() const
{ return error_code == ((0x33UL << 26) | 2); }

IMPLEMENT inline NEEDS["processor.h"]
Address_type
Jdb_entry_frame::from_user() const
{
  return (psr & Proc::Status_mode_mask) != Proc::Status_mode_supervisor
         ? ADDR_USER : ADDR_KERNEL;
}

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
Unsigned8*
Jdb_log_frame::str() const
{ return (Unsigned8*)r[1]; }

PUBLIC inline NEEDS["tb_entry.h"]
void
Jdb_log_frame::set_tb_entry(Tb_entry* tb_entry)
{ r[0] = (Mword)tb_entry; }

//---------------------------------------------------------------------------
PUBLIC inline
Mword
Jdb_log_3val_frame::val1() const
{ return r[2]; }

PUBLIC inline
Mword
Jdb_log_3val_frame::val2() const
{ return r[3]; }

PUBLIC inline
Mword
Jdb_log_3val_frame::val3() const
{ return r[4]; }

//---------------------------------------------------------------------------
PUBLIC inline
void
Jdb_status_page_frame::set(Address status_page)
{ r[0] = (Mword)status_page; }
