IMPLEMENTATION:

#include <cstdio>

#include "config.h"
#include "cpu.h"
#include "kdb_ke.h"
#include "kmem.h"
#include "logdefs.h"
#include "processor.h"
#include "std_macros.h"
#include "thread.h"
#include "warn.h"
#include "paging.h"


/** 
 * The global page fault handler switch.
 * Handles page-fault monitoring, classification of page faults based on
 * virtual-memory area they occurred in, page-directory updates for kernel
 * faults, IPC-window updates, and invocation of paging function for
 * user-space page faults (handle_page_fault_pager).
 * @param pfa page-fault virtual address
 * @param error_code CPU error code
 * @return true if page fault could be resolved, false otherwise
 * @exception longjmp longjumps to recovery location if page-fault
 *                    handling fails (i.e., return value would be false),
 *                    but recovery location has been installed
 */
IMPLEMENT inline NEEDS[<cstdio>,"kdb_ke.h","processor.h",
		       "config.h","std_macros.h","logdefs.h",
		       "warn.h",Thread::page_fault_log, "paging.h"]
int Thread::handle_page_fault(Address pfa, Mword error_code, Mword pc,
                              Return_frame *regs)
{
  CNT_PAGE_FAULT;

  // TODO: put this into a debug_page_fault_handler
  if (EXPECT_FALSE(log_page_fault()))
    page_fault_log(pfa, error_code, pc);

  L4_msg_tag ipc_code = L4_msg_tag(0, 0, 0, 0);

  // Check for page fault in user memory area
  if (EXPECT_TRUE(!Kmem::is_kmem_page_fault(pfa, error_code)))
    {
      // Make sure that we do not handle page faults that do not
      // belong to this thread.
      //assert (mem_space() == current_mem_space());

      if (EXPECT_FALSE(mem_space()->is_sigma0()))
        {
          // special case: sigma0 can map in anything from the kernel
	  if(handle_sigma0_page_fault(pfa))
            return 1;

          ipc_code.set_error();
          goto error;
        }

      // user mode page fault -- send pager request
      if (handle_page_fault_pager(_pager, pfa, error_code,
                                  L4_msg_tag::Label_page_fault))
        return 1;

      goto error;
    }

  // Check for page fault in kernel memory region caused by user mode
  else if (EXPECT_FALSE(PF::is_usermode_error(error_code)))
    return 0;             // disallow access after mem_user_max

  // We're in kernel code faulting on a kernel memory region

  // A page is not present but a mapping exists in the global page dir.
  // Update our page directory by copying from the master pdir
  // This is the only path that should be executed with interrupts
  // disabled if the page faulter also had interrupts disabled.   
  // thread_page_fault() takes care of that.
  else if (Mem_layout::is_caps_area(pfa))
    {
      // Test for special case -- see function documentation
      if (pagein_tcb_request(regs))
	 return 2;

      printf("Fiasco BUG: Invalid CAP access (pc=%lx, pfa=%lx)\n", pc, pfa);
      kdb_ke("Fiasco BUG: Invalid access to Caps area");
      return 0;
    }

  WARN("No page-fault handler for 0x%lx, error 0x%lx, pc " L4_PTR_FMT "\n",
        pfa, error_code, pc);

  // An error occurred.  Our last chance to recover is an exception
  // handler a kernel function may have set.
 error:

  longjmp_recover_jmpbuf();

  return 0;
}
