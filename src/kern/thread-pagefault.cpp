IMPLEMENTATION:

#include "kmem.h"
#include "paging.h"
#include "warn.h"


IMPLEMENT inline NEEDS["kmem.h", "paging.h", "warn.h", Thread::page_fault_log]
int Thread::handle_page_fault(Address pfa, Mword pf_info, Mword pc,
                              Return_frame *)
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
          // special case: sigma0 can map in anything from the kernel
	  if(handle_sigma0_page_fault(pfa))
            return 1;

          ipc_code.set_error();
          goto error;
        }

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

  longjmp_recover_jmpbuf();

  return 0;
}
