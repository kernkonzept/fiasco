IMPLEMENTATION:

#include "mem_unit.h"

extern "C" void sys_ipc_wrapper();
extern "C" void sys_ipc_log_wrapper();
extern "C" void sys_ipc_trace_wrapper();


typedef void (*Sys_call)();
extern "C" Unsigned32 sys_ipc_call_patch;

static inline void set_ipc(Sys_call c)
{
  // we directly patch the instruction stream
  Address p = (Address)c;
  // jal to c (c must be in the same 256MB aligned memory segment
  // like sys_ipc_call_patch
  sys_ipc_call_patch = 0x0c000000 | ((p >> 2) & 0xffffff);
  Mips::synci(&sys_ipc_call_patch);
}

IMPLEMENT void
Jdb_set_trace::ipc_tracing(Mode mode)
{
  switch (mode)
    {
    case Off:
      Jdb_ipc_trace::_trace = 0;
      Jdb_ipc_trace::_log = 0;
      Jdb_ipc_trace::_cshortcut = 0;
      Jdb_ipc_trace::_slow_ipc = 0;
      set_ipc(sys_ipc_wrapper);
      break;
    case Log:
      Jdb_ipc_trace::_trace = 0;
      Jdb_ipc_trace::_log = 1;
      Jdb_ipc_trace::_log_to_buf = 0;
      Jdb_ipc_trace::_cshortcut = 0;
      Jdb_ipc_trace::_slow_ipc = 0;
      set_ipc(sys_ipc_log_wrapper);
      break;
    case Log_to_buf:
      Jdb_ipc_trace::_trace = 0;
      Jdb_ipc_trace::_log = 1;
      Jdb_ipc_trace::_log_to_buf = 1;
      Jdb_ipc_trace::_cshortcut = 0;
      Jdb_ipc_trace::_slow_ipc = 0;
      set_ipc(sys_ipc_log_wrapper);
      break;
    case Trace:
      Jdb_ipc_trace::_trace = 1;
      Jdb_ipc_trace::_cshortcut = 0;
      Jdb_ipc_trace::_log = 0;
      Jdb_ipc_trace::_slow_ipc = 0;
      set_ipc(sys_ipc_trace_wrapper);
      break;
    case Use_c_short_cut:
      break;
    case Use_slow_path:
      Jdb_ipc_trace::_slow_ipc = 1;
      set_ipc(sys_ipc_wrapper);
      break;
    }
}

