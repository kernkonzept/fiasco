INTERFACE[ia32,amd64]:

#include "string_buffer.h"
#include "jdb_types.h"

class Jdb_entry_frame;

EXTENSION class Jdb_bp
{
private:
  static int		test_sstep(Cpu_number cpu, Jdb_entry_frame *ef);
  static int		test_break(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf);
  static int		test_other(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf);
  static int		test_log_only(Cpu_number cpu, Jdb_entry_frame *ef);
  static Mword		dr7;
};

#define write_debug_register(num, val) \
    asm volatile("mov %0, %%db" #num : : "r" ((Mword)val))

#define read_debug_register(num) \
    ({Mword val; asm volatile("mov %%db" #num ",%0" : "=r"(val)); val;})

IMPLEMENTATION[ia32,amd64]:

#include "kmem.h"

Mword Jdb_bp::dr7;

PUBLIC inline NEEDS["kmem.h"]
void
Breakpoint::set(Jdb_address _addr, Mword _len, Mode _mode, Log _log)
{
  addr = _addr;
  mode = _mode;
  log  = _log;
  len  = _len;
}

PUBLIC static inline
Mword
Jdb_bp::get_dr(Mword i)
{
  switch (i)
    {
    case 0: return read_debug_register(0);
    case 1: return read_debug_register(1);
    case 2: return read_debug_register(2);
    case 3: return read_debug_register(3);
    case 6: return read_debug_register(6);
    case 7: return dr7;
    default: return 0;
    }
}

IMPLEMENT
int
Jdb_bp::global_breakpoints()
{
  return 1;
}

static
int
Jdb_bp::set_debug_address_register(int num, Jdb_address addr, Mword len,
				   Breakpoint::Mode mode)
{
  clr_dr7(num, dr7);
  set_dr7(num, len, mode, dr7);
  switch (num)
    {
    case 0: write_debug_register(0, addr.addr()); break;
    case 1: write_debug_register(1, addr.addr()); break;
    case 2: write_debug_register(2, addr.addr()); break;
    case 3: write_debug_register(3, addr.addr()); break;
    default:;
    }
  return 1;
}

static
void
Jdb_bp::clr_debug_address_register(int num)
{
  clr_dr7(num, dr7);
}

IMPLEMENT
void
Jdb_bp::at_jdb_enter()
{
  dr7 = read_debug_register(7);
  // disable breakpoints while we are in kernel debugger
  write_debug_register(7, dr7 & Val_enter);
}

IMPLEMENT
void
Jdb_bp::at_jdb_leave()
{
  write_debug_register(6, Val_leave);
  write_debug_register(7, dr7);
}

/** @return 1 if single step occurred */
IMPLEMENT
int
Jdb_bp::test_sstep(Cpu_number, Jdb_entry_frame *)
{
  Mword dr6 = read_debug_register(6);
  if (!(dr6 & Val_test_sstep))
    return 0;

  // single step has highest priority, don't consider other conditions
  write_debug_register(6, Val_leave);
  return 1;
}

/** @return 1 if breakpoint occurred */
IMPLEMENT
int
Jdb_bp::test_break(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf)
{
  Mword dr6 = read_debug_register(6);
  if (!(dr6 & Val_test))
    return 0;

  int ret = test_break(cpu, ef, buf, dr6);
  write_debug_register(6, dr6 & ~Val_test);
  return ret;
}

/** @return 1 if other debug exception occurred */
IMPLEMENT
int
Jdb_bp::test_other(Cpu_number, Jdb_entry_frame *, String_buffer *buf)
{
  Mword dr6 = read_debug_register(6);
  if (!(dr6 & Val_test_other))
    return 0;

  buf->printf("unknown trap 1 (dr6=" L4_PTR_FMT ")", dr6);
  write_debug_register(6, Val_leave);
  return 1;
}

/** @return 1 if only breakpoints were logged and jdb should not be entered */
IMPLEMENT
int
Jdb_bp::test_log_only(Cpu_number, Jdb_entry_frame *)
{
  Mword dr6 = read_debug_register(6);

  if (dr6 & Val_test)
    {
      dr7 = read_debug_register(7);
      // disable breakpoints -- we might trigger a r/w breakpoint again
      write_debug_register(7, dr7 & Val_enter);
      test_log(dr6);
      write_debug_register(6, dr6);
      write_debug_register(7, dr7);
      if (!(dr6 & Val_test_other))
	// don't enter jdb, breakpoints only logged
	return 1;
    }
  // enter jdb
  return 0;
}

IMPLEMENT
void
Jdb_bp::init_arch()
{
  Jdb::bp_test_log_only = test_log_only;
  Jdb::bp_test_break    = test_break;
  Jdb::bp_test_sstep    = test_sstep;
  Jdb::bp_test_other    = test_other;
}
