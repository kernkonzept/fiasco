INTERFACE:

#include "string_buffer.h"

EXTENSION class Jdb_bp
{
private:
  static int		test_log_only();
  static int		test_break(String_buffer *buf);
};


IMPLEMENTATION[ux]:

#include <sys/types.h>			// for pid_t
#include "boot_info.h"
#include "task.h"
#include "usermode.h"
#include "jdb_kobject.h"

PUBLIC inline
Mword
Breakpoint::restricted_task()
{
  return restrict.task.task;
}

PUBLIC inline
void
Breakpoint::set(Address _addr, Mword _len, Mode _mode, Log _log)
{
  addr = _addr;
  mode = _mode;
  user = ADDR_USER;   // we don't allow breakpoints in kernel space
  log  = _log;
  len  = _len;
}

IMPLEMENT
int
Jdb_bp::global_breakpoints()
{
  return 0;
}

PUBLIC static
Mword
Jdb_bp::read_debug_register(int reg, Space *s)
{
  pid_t pid = s ? s->pid() : Boot_info::pid();
  Mword val;
  if (!Usermode::read_debug_register(pid, reg, val))
    printf("[read debugreg #%d task %p/%lx failed]\n", reg, s,
           static_cast<Task*>(s)->dbg_info()->dbg_id());

  return val;
}

static
void
Jdb_bp::write_debug_register(int reg, Mword val, Space *s)
{
  pid_t pid = s ? s->pid() : Boot_info::pid();
  if (!Usermode::write_debug_register(pid, reg, val))
    printf("[write %08lx to debugreg #%d task %p/%lx failed]\n", val, reg, s,
           static_cast<Task*>(s)->dbg_info()->dbg_id());
}

PUBLIC static inline
Mword
Jdb_bp::get_debug_control_register(Space *task)
{
  return read_debug_register(7, task);
}

PUBLIC static inline
void
Jdb_bp::set_debug_control_register(Mword val, Space *task)
{
  printf("set_debug_control_register\n");
  for (int i=0; i<4; i++)
    if (!(val & (2 << 2*i)))
      val &= ~(0x0f << (16 + 4*i));
  write_debug_register(7, val, task);
}

static
int
Jdb_bp::set_debug_address_register(int num, Mword addr, Mword len,
				   Breakpoint::Mode mode, Task *task)
{
  if (!task)
    {
      putstr(" => kernel task not allowed for breakpoints");
      return 0;
    }
  if (num >= 0 && num <= 3)
    {
      Mword local_dr7;
      Task *old_task = cxx::dyn_cast<Task*>(Kobject::from_dbg(Kobject_dbg::id_to_obj(bps[num].restricted_task())));

      if (old_task)
	{
	  // clear old breakpoint of other process
	  local_dr7 = get_debug_control_register(old_task);
	  clr_dr7(num, local_dr7);
	  set_debug_control_register(local_dr7, old_task);
	}
      bps[num].restrict_task(0, task->dbg_info()->dbg_id());
      write_debug_register(num, addr, task);
      local_dr7 = get_debug_control_register(task);
      clr_dr7(num, local_dr7);
      set_dr7(num, len, mode, local_dr7);
      set_debug_control_register(local_dr7, task);
      return 1;
    }

  return 0;
}

static
void
Jdb_bp::clr_debug_address_register(int num)
{
  Task *task = cxx::dyn_cast<Task*>(Kobject::from_dbg(Kobject_dbg::id_to_obj(bps[num].restricted_task())));
  Mword local_dr7 = get_debug_control_register(task);
  clr_dr7(num, local_dr7);
  set_debug_control_register(local_dr7, task);
}

IMPLEMENT
void
Jdb_bp::at_jdb_enter()
{}

IMPLEMENT
void
Jdb_bp::at_jdb_leave()
{
  if (Jdb::get_current_active()
      && Jdb::get_current_active()->space() != Kernel_task::kernel_task())
    write_debug_register(6, 0, Jdb::get_current_active()->space());
}

IMPLEMENT
int
Jdb_bp::test_log_only()
{
  Space *t = Jdb::get_thread(Cpu_number::boot_cpu())->space();
  Mword dr6  = read_debug_register(6, t);

  if (dr6 & 0x0000000f)
    {
      test_log(dr6);
      write_debug_register(6, dr6, t);
      if (!(dr6 & 0x0000e00f))
	// don't enter jdb, breakpoints only logged
	return 1;
    }
  // enter jdb
  return 0;
}

/** @return 1 if breakpoint occured */
IMPLEMENT
int
Jdb_bp::test_break(String_buffer *buf)
{
  Space *t = Jdb::get_thread(Cpu_number::boot_cpu())->space();
  Mword dr6  = read_debug_register(6, t);

  if (!(dr6 & 0x000000f))
    return 0;

  test_break(buf, dr6);
  write_debug_register(6, dr6 & ~0x0000000f, t);
  return 1;
}

IMPLEMENT
void
Jdb_bp::init_arch()
{
  Jdb::bp_test_log_only = test_log_only;
  Jdb::bp_test_break    = test_break;
}

