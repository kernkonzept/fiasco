IMPLEMENTATION [riscv]:

#include "thread.h"

PRIVATE static inline NEEDS["thread.h"]
void
Jdb_set_trace::set_ipc_entry(Thread::Sys_call e)
{
  Thread::set_sys_call_entry(e);
}

extern "C" void sys_ipc_wrapper (void);
extern "C" void sys_ipc_log_wrapper (void);

IMPLEMENT
void
Jdb_set_trace::ipc_tracing(Mode mode)
{
  switch (mode)
    {
    case Off:
      Jdb_ipc_trace::_log = 0;
      set_ipc_entry(sys_ipc_wrapper);
      break;
    case Log:
      Jdb_ipc_trace::_log = 1;
      Jdb_ipc_trace::_log_to_buf = 0;
      set_ipc_entry(sys_ipc_log_wrapper);
      break;
    case Log_to_buf:
      Jdb_ipc_trace::_log = 1;
      Jdb_ipc_trace::_log_to_buf = 1;
      set_ipc_entry(sys_ipc_log_wrapper);
      break;
    }
}
