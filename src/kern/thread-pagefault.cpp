IMPLEMENTATION:

#include "kmem.h"
#include "paging.h"
#include "warn.h"


IMPLEMENT inline NEEDS["kmem.h", "paging.h", "warn.h", Thread::page_fault_log,
                       Thread::handle_page_fault_trampoline]
int Thread::handle_page_fault(Address pfa, Mword pf_info, Mword pc,
                              Trap_state *ts, bool release_cpulock)
{
  CNT_PAGE_FAULT;

  // TODO: put this into a debug_page_fault_handler
  if (log_page_fault()) [[unlikely]]
    page_fault_log(pfa, pf_info, pc);

  L4_msg_tag ipc_code = L4_msg_tag(0, 0, 0, 0);

  // Check for page fault in user memory area
  if (!Kmem::is_kmem_page_fault(pfa, pf_info)) [[likely]]
    {
      // Make sure that we do not handle page faults that do not
      // belong to this thread.
      //assert (mem_space() == current_mem_space());

      if (mem_space()->is_sigma0()) [[unlikely]]
        {
          if (release_cpulock)
            Proc::sti();

          // special case: sigma0 can map in anything from the kernel
	  if (handle_sigma0_page_fault(pfa))
            return 1;

          ipc_code.set_error();
          goto error;
        }

      // Try one-shot, in-thread handler first
      if (handle_page_fault_trampoline(pfa, pf_info, ts))
        return 1;

      // Maybe not necessary because handle_page_fault_pager() acquires the CPU
      // lock again. But this is at least a preemption point.
      if (release_cpulock)
        Proc::sti();

      // user mode page fault -- send pager request
      if (handle_page_fault_pager(_pager, pfa, pf_info,
                                  L4_msg_tag::Label_page_fault))
        return 1;

      goto error;
    }

  // don't allow page fault in kernel memory region caused by user mode
  else if (PF::is_usermode_error(pf_info)) [[unlikely]]
    return 0;

  // We're in kernel code faulting on a kernel memory region
  WARN("No page-fault handler for 0x%lx, error/info 0x%lx, pc " L4_PTR_FMT "\n",
        pfa, pf_info, pc);

  // An error occurred.  Our last chance to recover is an exception
  // handler a kernel function may have set.
 error:

  if (release_cpulock)
    Proc::sti();

  longjmp_recover_jmpbuf();

  return 0;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [!pagefault_trampoline]:

PRIVATE inline
bool
Thread::handle_page_fault_trampoline(Address, Mword, Trap_state *)
{
  return false;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [pagefault_trampoline]:

#include "cpu_lock.h"

PRIVATE
bool
Thread::handle_page_fault_trampoline(Address pfa, Mword pf_info, Trap_state *ts)
{
  if (!(state() & Thread_pf_trampoline))
    return false;

  assert(cpu_lock.test());

  state_del_dirty(Thread_pf_trampoline);

  // Actually only required for ARM32/novirt (SP_usr/LR_usr) and ARM64 (SP_EL0).
  // Must be done here so that tramp_state->ts contains the information.
  // The supplemental fill_user_state() is done directly in the ARM32/ARM64
  // resume_{normal,trigger_exception}_after_pf_trampoline() Assembler code.
  spill_user_state();

  Pf_trampoline *tramp_state = pf_tramp_state().access();
  tramp_state->ts = *ts;
  tramp_state->saved_mr0 = this->utcb().access(true)->values[0];
  tramp_state->flags |= Pf_trampoline::Pf_in_progress;
  ts->ip(tramp_state->ip);

  arch_return_to_pf_trampoline(pfa, pf_info, ts,
                               tramp_state, pf_tramp_state().usr());

  return true;
}
