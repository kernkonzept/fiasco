INTERFACE:

#include "thread.h"

class kernel_thread_t : public thread_t
{
};


IMPLEMENTATION:

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <flux/machine/base_trap.h>
#include <flux/machine/proc_reg.h>
#include <flux/machine/pc/pic.h>
#include <flux/machine/pc/rtc.h>
#include <flux/machine/pc/com_cons.h>

#include "config.h"
#include "console.h"
#include "space.h"
#include "thread.h"
#include "thread_state.h"
#include "kdb_ke.h"
#include "kmem.h"
#include "kmem_alloc.h"
#include "irq.h"
#include "globals.h"
#include "helping_lock.h"

// overload allocator -- we cannot allcate by page fault during
// bootstrapping
PUBLIC
void *
kernel_thread_t::operator new(size_t s, threadid_t id) throw()
{
  // call superclass' allocator
  void *addr = thread_t::operator new(s, id);

  // explicitly allocate and enter in page table -- we cannot allocate
  // by page fault during bootstrapping
  if (! kmem_alloc::page_alloc(reinterpret_cast<vm_offset_t>(addr) 
			         & ~PAGE_MASK,
			       kmem_alloc::zero_fill))
    panic("can't allocate kernel tcb");

  return addr;
}

PUBLIC inline NEEDS["kmem.h"]
kernel_thread_t::kernel_thread_t()
  : thread_t (// XXX ugly, but OK for now -- perhaps spaces and kmem should be
	      // inherited from a common super class
	      const_cast<space_t*>
	        (reinterpret_cast<const space_t*> (kmem::dir())),
	      &config::kernel_id
	      )
{}

PUBLIC inline
unsigned long *
kernel_thread_t::init_stack() 
{
  return kernel_sp; 
}

// the kernel bootstrap routine
PUBLIC
void 
kernel_thread_t::bootstrap()
{
  // Initializations done -- helping_lock_t can now use helping lock
  helping_lock_t::threading_system_active = true;

  //
  // set up my own thread control block
  //
  state_change (0, Thread_running);

  sched()->set_prio (config::kernel_prio);
  sched()->set_mcp (config::kernel_mcp);
  sched()->set_timeslice (config::default_time_slice);
  sched()->set_ticks_left (config::default_time_slice);

  present_next = present_prev = this;
  ready_next = ready_prev = this;

  //
  // set up class variables
  //
  for (int i = 0; i < 256; i++) prio_next[i] = prio_first[i] = 0;
  prio_next[config::kernel_prio] = prio_first[config::kernel_prio] = this;
  prio_highest = config::kernel_prio;

  timeslice_ticks_left = config::default_time_slice;
  timeslice_owner = this;

  // 
  // install our slow trap handler
  //
  nested_trap_handler = base_trap_handler;
  base_trap_handler = thread_handle_trap;

  // 
  // initialize FPU
  // 
  set_ts();			// FPU ops -> exception

  //
  // initialize interrupts
  //
  irq_t::lookup(2)->alloc(this, false); // reserve cascade irq
  irq_t::lookup(8)->alloc(this, false); // reserve timer irq

  pic_enable_irq(2);		// allow cascaded irqs

  // set up serial console
  if (! strstr(kmem::cmdline(), " -I-") 
      && !strstr(kmem::cmdline(), " -irqcom"))
    {
      int com_port = console::serial_com_port;
      int com_irq = com_port & 1 ? 4 : 3;

      irq_t::lookup(com_irq)->alloc(this, false); // the remote-gdb interrupt
      pic_enable_irq(com_irq);

      // for some reason, we have to re-enable the com irq here
      if (config::serial_esc)
	com_cons_enable_receive_interrupt();
    }

  // initialize the profiling timer
  bool user_irq0 = strstr(kmem::cmdline(), "irq0");

  if (config::profiling)
    {
      if (user_irq0)
	{
	  kdb_ke("options `-profile' and `-irq0' don't mix "
		  "-- disabling `-irq0'");
	}
      irq_t::lookup(0)->alloc(this, false);

      profile::init();

      if (strstr(kmem::cmdline(), " -profstart"))
	profile::start();
    }
  else if (! user_irq0)
    irq_t::lookup(0)->alloc(this, false); // reserve irq0 even though
                                          // we don't use it

  // 
  // set up timer interrupt (~ 1ms)
  //
  while (rtcin(RTC_STATUSA) & RTCSA_TUP) ; // wait till RTC ready
  rtcout(RTC_STATUSA, RTCSA_DIVIDER | RTCSA_1024); // 1024 Hz
  // set up 1024 Hz interrupt
  rtcout(RTC_STATUSB, rtcin(RTC_STATUSB) | RTCSB_PINTR | RTCSB_SQWE); 
  rtcin(RTC_INTR);		// reset

  pic_enable_irq(8);		// allow this interrupt

  //
  // set PCE-Flag in CR4 to enable read of performace measurement counters
  // in usermode. PMC were introduced in Pentium MMX and PPro processors.
  //
  #ifndef CPUF_MMX
  #define CPUF_MMX 0x00800000
  #endif
  if(strncmp(cpu.vendor_id, "GenuineIntel", 12) == 0
     && (cpu.family == CPU_FAMILY_PENTIUM_PRO ||
	 cpu.feature_flags & CPUF_MMX))
    {
      set_cr4(get_cr4() | CR4_PCE);
    }
  
  //
  // allow the boot task to create more tasks
  //
  for (unsigned i = config::boot_taskno + 1; 
       i < space_index_t::max_space_number;
       i++)
    {
      check(space_index_t(i).set_chief(space_index(), 
				       space_index_t(config::boot_taskno)));
    }

  //
  // create sigma0
  //

  // sigma0's chief is the boot task
  space_index_t(config::sigma0_id.id.task).
    set_chief(space_index(), space_index_t(config::sigma0_id.id.chief));

  sigma0 = new space_t(config::sigma0_id.id.task);
  sigma0_thread = 
    new (&config::sigma0_id) thread_t (sigma0, &config::sigma0_id, 
				       config::sigma0_prio, 
				       config::sigma0_mcp);
  
  // push address of kernel info page to sigma0's stack
  vm_offset_t esp = kmem::info()->sigma0_esp;

  * reinterpret_cast<vm_offset_t*>(kmem::phys_to_virt(--esp)) 
    = kmem::virt_to_phys(kmem::info());

  sigma0_thread->initialize(kmem::info()->sigma0_eip, esp,
			    0, 0);

  //
  // create the boot task
  //

  // the boot task's chief is the boot task itself
  space_index_t(config::boot_id.id.task).
    set_chief(space_index(), space_index_t(config::boot_id.id.chief));

  space_t *boot = new space_t(config::boot_id.id.task);
  thread_t *boot_thread
    = new (&config::boot_id) thread_t (boot, &config::boot_id, 
				       config::boot_prio, 
				       config::boot_mcp);

  boot_thread->initialize(0x200000,
			  0x200000,
			  sigma0_thread, 0);

  //
  // the idle loop
  //
  for (;;) 
    {
      // printf("I");

      sti();			// enable irqs, otherwise idling is fatal

      if (config::hlt_works_ok)
	asm("hlt");		// stop the CPU, waiting for an int

      while (ready_next != this) // are there any other threads ready?
	schedule();
    }
}

// A C interface for bootstrap, callable from assembly code
extern "C"
void call_bootstrap(kernel_thread_t *t)
{
  t->bootstrap();
}
