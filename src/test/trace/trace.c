#include <l4/sys/ipc.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/ktrace.h>
#include <l4/rmgr/librmgr.h>

#include <stdio.h>

#include <l4/util/rdtsc.h>
#include <l4/util/thread.h>
#include <l4/util/util.h>
#include <flux/x86/pc/reset.h>

typedef struct __attribute__((packed))
{
  unsigned number;			// event number
  unsigned eip;			// instruction pointer
  l4_threadid_t tid;		// thread id
  unsigned long long tsc;		// time stamp counter
  unsigned long long pmc;		// performance counter value
  union __attribute__((packed))	// data for each type of entry
    {
      struct __attribute__((packed))
	{
	  // logged ipc
	  unsigned eax, edx, ebx, ebp;
	  l4_threadid_t dest;
	} ipc;
      struct __attribute__((packed))
	{
	  // logged ipc result
	  unsigned eax, edx, ebx;
	  unsigned orig_eax, orig_ebp;
	} ipc_res;
      struct __attribute__((packed))
	{
	  // traced ipc
	  unsigned long long send_tsc;
	  l4_threadid_t send_esi_edi;
	  l4_threadid_t recv_esi_edi;
	  l4_msgdope_t dope;
	  unsigned char send_desc, recv_desc;
	} ipc_trace;
      struct __attribute__((packed))
	{
	  // logged page fault
	  unsigned pfa;
	  unsigned err;
	} pf;
      struct __attribute__((packed))
	{
	  // logged kernel event
	  char msg[31];
	} ke;
      struct __attribute__((packed)) 
	{ 
	  // logged kernel event plus register content
	  unsigned eax, edx, ecx;
	  char msg[19];
	} ke_reg;
      struct __attribute__((packed))
	{
	  // logged unmap operation
	  l4_fpage_t fpage;
	  unsigned mask;
	  int result;
	} unmap;
      struct __attribute__((packed))
	{
	  unsigned lthread;
	  unsigned old_esp;
	  unsigned new_esp;
	  unsigned old_eip;
	  unsigned new_eip;
	} ex_regs;
      struct
	{
	  // logged short-cut ipc failed
	  unsigned eax, ebp, ecx;
	  l4_threadid_t dest;
	  char is_irq, snd_lst, dst_ok, dst_lck;
	} ipc_sfl;
      struct
	{
	  // logged context switch
	  l4_threadid_t dest;
	  unsigned kernel_eip;
	  unsigned long *kernel_sp;
	} ctx_sw;
    } m;
  char type;	// type of entry
} tb_entry_t;

static int ticker_thread_stack[1024];
static l4_tracebuffer_status_t *tbuf;
static tb_entry_t *tbuffer[2];

static void
init_tbuf(void)
{
  // request address of trace buffer from kernel
  if (!(tbuf = fiasco_get_tbuf_status()))
    enter_kdebug("no kernel trace buffer");

  printf("buf0: %08x, size0: %08x; buf1: %08x, size1: %08x\n",
      tbuf->tracebuffer0, tbuf->size0,
      tbuf->tracebuffer1, tbuf->size1);

  tbuffer[0] = (tb_entry_t*)tbuf->tracebuffer0;
  tbuffer[1] = (tb_entry_t*)tbuf->tracebuffer1;
}

// do something we can log
static void
ticker_thread(void)
{
  int i;
  enter_kdebug("*#I*");
  enter_kdebug("*#IrT7.1");
  for (;;)
    {
      for (i=0; i<100; i++)
	{
	  l4_sleep(10);
	}
    }
}

static void
create_ticker_thread(void)
{
  create_thread(l4_myself().id.lthread+1, 
                ticker_thread,
		&ticker_thread_stack[1024]);
}

// wait for virtual irq which is released on trace buffer overflow
static void
wait_for_virq(void)
{
  int error;
  l4_msgdope_t result;
  l4_threadid_t virq_id;
  unsigned msg0, msg1;

  virq_id.lh.high = 0; 
  virq_id.lh.low  = 17;

  if ((error = l4_i386_ipc_receive(virq_id,
				   L4_IPC_SHORT_MSG, &msg0, &msg1,
				   L4_IPC_TIMEOUT(0,0,0,1,0,0),
				   &result)) != L4_IPC_RETIMEOUT)
    {
      printf("Error %02x allocating interrupt\n", error);
      return;
    }
  
  for (;;)
    {
      int i, j;
      int old_version0 = tbuf->version0;
      int old_version1 = tbuf->version1;
      tb_entry_t *tbe;

      printf("Waiting for interrupts...");
      error = l4_i386_ipc_receive(virq_id, L4_IPC_SHORT_MSG,
				  &msg0, &msg1,
				  L4_IPC_NEVER, &result);

      printf("Received interrupt\n");

      printf("version0: %08x, version1: %08x\n",
	  tbuf->version0, tbuf->version1);

      if (tbuf->version0 != old_version0)
	{
	  j=tbuf->size0/sizeof(tb_entry_t);
	  tbe = tbuffer[0];
	}
      else
	{
	  j=tbuf->size1/sizeof(tb_entry_t);
	  tbe = tbuffer[1];
	}

      old_version0 = tbuf->version0;
      old_version1 = tbuf->version1;

      for(i=0; i<j; i++)
	{
       	  unsigned *tsc = (unsigned *) &tbe[i].tsc;
	  printf("nr: %08x, type: %d, tid: %x.%02x, eip: %08x, %08x:%08x\n",
	      tbe[i].number, tbe[i].type, 
	      tbe[i].tid.id.task,
	      tbe[i].tid.id.lthread,
	      tbe[i].eip,
	      tsc[1], tsc[0]);
	}

      if (   (old_version0 != tbuf->version0)
	  || (old_version1 != tbuf->version1))
  	printf("version changed! version0: %08x, version1: %08x\n",
  	    tbuf->version0, tbuf->version1);
    }
}

int
main(int argc, char **argv) 
{
  init_tbuf();

  create_ticker_thread();
  
  wait_for_virq();

  return 0;
}

