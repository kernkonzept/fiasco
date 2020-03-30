IMPLEMENTATION[ia32,ux]:

static
void
set_fast_entry(Cpu_number cpu, void (*fast_entry)(void))
{
  Cpu::cpus.cpu(cpu).set_fast_entry(fast_entry);
}

//--------------------------------------------------------------------------
IMPLEMENTATION[amd64 && kernel_isolation]:

#include "jdb.h"
#include "jdb_types.h"

static
void
set_fast_entry(Cpu_number, void (*func)())
{
  extern char const syscall_entry_code[];
  extern char const syscall_entry_reloc[];
  // 3 byte offset into "mov fn,%r11" instruction
  auto ofs = syscall_entry_reloc - syscall_entry_code + 3;
  auto reloc = reinterpret_cast<Signed32 *>(
    Mem_layout::Mem_layout::Kentry_cpu_syscall_entry + ofs);
  check(Jdb::poke(Jdb_addr<Signed32>::kmem_addr(reloc), (Signed32)(Signed64)func));
}

//--------------------------------------------------------------------------
IMPLEMENTATION[amd64 && !kernel_isolation]:

#include "jdb.h"
#include "jdb_types.h"

static
void
set_fast_entry(Cpu_number cpu, void (*func)())
{
  extern Per_cpu_array<Syscall_entry_text> syscall_entry_text;

  Address entry = (Address)&syscall_entry_text[cpu];
  Address reloc = entry + 0x1b;
  Signed32 ofs = (Address)func - (reloc + sizeof(Signed32));
  check(Jdb::poke(Jdb_addr<Signed32>::kmem_addr((Signed32 *) reloc), ofs));
}

//--------------------------------------------------------------------------
IMPLEMENTATION:

#include "syscalls.h"
#include "jdb.h"
#include "pm.h"

extern "C" void sys_ipc_wrapper (void);
extern "C" void sys_ipc_log_wrapper (void);
extern "C" void sys_ipc_trace_wrapper (void);

extern "C" void entry_sys_fast_ipc_log (void);
extern "C" void entry_sys_fast_ipc_c (void);

static
void
Jdb_set_trace::set_ipc_vector()
{
  void (*fast_entry)(void);

  if (Jdb_ipc_trace::_trace || Jdb_ipc_trace::_slow_ipc || Jdb_ipc_trace::_log)
    fast_entry  = entry_sys_fast_ipc_log;
  else
    fast_entry  = entry_sys_fast_ipc_c;

  Jdb::on_each_cpu([fast_entry](Cpu_number cpu){
    set_fast_entry(cpu, fast_entry);
  });

  set_ipc_vector_int();

  if (Jdb_ipc_trace::_trace)
    syscall_table[0] = sys_ipc_trace_wrapper;
  else if ((Jdb_ipc_trace::_log && !Jdb_ipc_trace::_slow_ipc))
    syscall_table[0] = sys_ipc_log_wrapper;
  else
    syscall_table[0] = sys_ipc_wrapper;
}

IMPLEMENT void
Jdb_set_trace::ipc_tracing(Mode mode)
{
  switch (mode)
    {
    case Off:
      Jdb_ipc_trace::_trace = 0;
      Jdb_ipc_trace::_log = 0;
      Jdb_ipc_trace::_slow_ipc = 0;
      break;
    case Log:
      Jdb_ipc_trace::_trace = 0;
      Jdb_ipc_trace::_log = 1;
      Jdb_ipc_trace::_log_to_buf = 0;
      Jdb_ipc_trace::_slow_ipc = 0;
      break;
    case Log_to_buf:
      Jdb_ipc_trace::_trace = 0;
      Jdb_ipc_trace::_log = 1;
      Jdb_ipc_trace::_log_to_buf = 1;
      Jdb_ipc_trace::_slow_ipc = 0;
      break;
    case Trace:
      Jdb_ipc_trace::_trace = 1;
      Jdb_ipc_trace::_log = 0;
      Jdb_ipc_trace::_slow_ipc = 0;
      break;
    case Use_slow_path:
      Jdb_ipc_trace::_slow_ipc = 1;
      break;
    }
  set_ipc_vector();
}

namespace {
struct Jdb_ipc_log_pm : Pm_object
{
  Jdb_ipc_log_pm(Cpu_number cpu) { register_pm(cpu); }
  void pm_on_resume(Cpu_number cpu) override
  {
    void (*fast_entry)(void);

    if (Jdb_ipc_trace::_trace || Jdb_ipc_trace::_slow_ipc ||
        Jdb_ipc_trace::_log)
      fast_entry  = entry_sys_fast_ipc_log;
    else
      fast_entry  = entry_sys_fast_ipc_c;

    set_fast_entry(cpu, fast_entry);
  }

  void pm_on_suspend(Cpu_number) override {}
};

DEFINE_PER_CPU static Per_cpu<Jdb_ipc_log_pm> _pm(Per_cpu_data::Cpu_num);

}

//--------------------------------------------------------------------------
IMPLEMENTATION [ia32,ux]:

#include "idt.h"

extern "C" void entry_sys_ipc_log (void);
extern "C" void entry_sys_ipc_c (void);

static
void
Jdb_set_trace::set_ipc_vector_int()
{
  void (*int30_entry)(void);

  if (Jdb_ipc_trace::_trace || Jdb_ipc_trace::_slow_ipc || Jdb_ipc_trace::_log)
    int30_entry = entry_sys_ipc_log;
  else
    int30_entry = entry_sys_ipc_c;

  Idt::set_entry(0x30, (Address) int30_entry, true);
}

IMPLEMENTATION [amd64]:

static
void
Jdb_set_trace::set_ipc_vector_int()
{
}
