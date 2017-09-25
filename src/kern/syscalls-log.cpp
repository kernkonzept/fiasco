IMPLEMENTATION [log]:

#include <cstring>
#include "config.h"
#include "jdb_trace.h"
#include "jdb_tbuf.h"
#include "types.h"
#include "cpu_lock.h"

extern "C" void sys_ipc_wrapper(void);
extern "C" void sys_ipc_log_wrapper(void);


/** IPC logging.
    called from interrupt gate.
 */
IMPLEMENT void FIASCO_FLATTEN sys_ipc_log_wrapper()
{
  Thread *curr = current_thread();
  Entry_frame   *regs      = curr->regs();
  Syscall_frame *ipc_regs  = regs->syscall_frame();

  Mword entry_event_num    = (Mword)-1;
  Unsigned8 have_snd       = (ipc_regs->ref().op() & L4_obj_ref::Ipc_send)
                             || (ipc_regs->ref().op() == L4_obj_ref::Ipc_call);
  Utcb *utcb = curr->utcb().access(true);
  int do_log               = Jdb_ipc_trace::log() &&
				Jdb_ipc_trace::check_restriction (curr->dbg_id(),
					 static_cast<Task*>(curr->space())->dbg_id(),
					 ipc_regs, 0);

  if (do_log)
    {
      Mword dbg_id;
	{
	  Obj_cap r = ipc_regs->ref();
          L4_fpage::Rights rights;
	  Kobject_iface *o = r.deref(&rights, true);
	  if (o)
	    dbg_id = o->dbg_info()->dbg_id();
	  else
	    dbg_id = ~0UL;
	}
      Tb_entry_ipc _local;
      Tb_entry_ipc *tb = EXPECT_TRUE(Jdb_ipc_trace::log_buf())
                       ? Jdb_tbuf::new_entry<Tb_entry_ipc>()
                       : &_local;
      tb->set(curr, regs->ip(), ipc_regs, utcb,
	      dbg_id, curr->sched_context()->left());

      entry_event_num = tb->number();

      if (EXPECT_TRUE(Jdb_ipc_trace::log_buf()))
	Jdb_tbuf::commit_entry();
      else
	Jdb_tbuf::direct_log_entry(tb, "IPC");
    }


  // now pass control to regular sys_ipc()
  sys_ipc_wrapper();

  if (Jdb_ipc_trace::log() && Jdb_ipc_trace::log_result() && do_log)
    {
      Tb_entry_ipc_res _local;
      Tb_entry_ipc_res *tb = static_cast<Tb_entry_ipc_res*>
	(EXPECT_TRUE(Jdb_ipc_trace::log_buf()) ? Jdb_tbuf::new_entry()
					    : &_local);
      tb->set(curr, regs->ip(), ipc_regs, utcb, utcb->error.raw(),
	      entry_event_num, have_snd, false);

      if (EXPECT_TRUE(Jdb_ipc_trace::log_buf()))
	Jdb_tbuf::commit_entry();
      else
	Jdb_tbuf::direct_log_entry(tb, "IPC result");
    }
}

/** IPC tracing.
 */
extern "C" void sys_ipc_trace_wrapper(void);
extern "C" void sys_ipc_wrapper();



IMPLEMENT void FIASCO_FLATTEN sys_ipc_trace_wrapper()
{
  Thread *curr = current_thread();
  Entry_frame *ef      = curr->regs();
  Syscall_frame *regs  = ef->syscall_frame();

  //Mword      from_spec = regs->from_spec();
  L4_obj_ref snd_dst   = regs->ref();

  Unsigned64 orig_tsc  = Cpu::rdtsc();

  // first try the fastpath, then the "slowpath"
  sys_ipc_wrapper();

  // kernel is locked here => no Lock_guard <...> needed
  Tb_entry_ipc_trace *tb =
    static_cast<Tb_entry_ipc_trace*>(Jdb_tbuf::new_entry());

  tb->set(curr, ef->ip(), orig_tsc, snd_dst, regs->from_spec(),
          L4_msg_tag(0,0,0,0), 0, 0);

  Jdb_tbuf::commit_entry();
}


