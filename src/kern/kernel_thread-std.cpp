IMPLEMENTATION:

#include "assert_opt.h"
#include "config.h"
#include "factory.h"
#include "initcalls.h"
#include "ipc_gate.h"
#include "irq.h"
#include "kip.h"
#include "koptions.h"
#include "map_util.h"
#include "mem_layout.h"
#include "sigma0_task.h"
#include "task.h"
#include "thread_object.h"
#include "types.h"
#include "ram_quota.h"

IMPLEMENT
void
Kernel_thread::init_workload()
{
  Cap_index const C_task    = Cap_index(Initial_kobjects::Task);
  Cap_index const C_factory = Cap_index(Initial_kobjects::Factory);
  Cap_index const C_thread  = Cap_index(Initial_kobjects::Thread);
  Cap_index const C_pager   = Cap_index(Initial_kobjects::Pager);

  auto g = lock_guard(*cpu_lock);

  //
  // create sigma0
  //

  int err;
  Task *sigma0 = Task::create<Sigma0_task>(*Ram_quota::root, L4_msg_tag(), 0, 0, &err, 0);

  assert_opt (sigma0);
  L4_fpage sigma0_utcb = L4_fpage::mem(Mem_layout::Utcb_addr,
                                       Config::PAGE_SHIFT);
  check(sigma0->alloc_ku_mem(sigma0_utcb)
        >= 0);
  sigma0->map_all_segs(Mem_desc::Dedicated);
  // prevent deletion of this thing
  sigma0->inc_ref();

  init_mapdb_mem(sigma0);
  init_mapdb_io(sigma0);

  check (map(sigma0,          sigma0, sigma0, C_task, 0));
  check (map(Factory::root(), sigma0, sigma0, C_factory, 0));

  for (Cap_index c = Initial_kobjects::first(); c < Initial_kobjects::end(); ++c)
    {
      Kobject_iface *o = initial_kobjects->obj(c);
      if (o)
	check(map(o, sigma0, sigma0, c, 0));
    }

  Thread_object *sigma0_thread = new (*Ram_quota::root) Thread_object(*Ram_quota::root);

  assert(sigma0_thread);

  // prevent deletion of this thing
  sigma0_thread->inc_ref();
  check (map(sigma0_thread, sigma0, sigma0, C_thread, 0));

  check (sigma0_thread->control(Thread_ptr(Thread_ptr::Null), Thread_ptr(Thread_ptr::Null)) == 0);
  check (sigma0_thread->bind(sigma0, User<Utcb>::Ptr((Utcb*)Virt_addr::val(sigma0_utcb.mem_address()))));
  check (sigma0_thread->ex_regs(Kip::k()->sigma0_ip, 0));

  //
  // create the boot task
  //

  Task *boot_task = Task::create<Task>(*Ram_quota::root, L4_msg_tag(), 0, 0, &err, 0);

  assert_opt (boot_task);
  L4_fpage root_utcb = L4_fpage::mem(Mem_layout::Utcb_addr,
                                     Config::PAGE_SHIFT);
  check(boot_task->alloc_ku_mem(root_utcb)
        >= 0);
  boot_task->map_all_segs(Mem_desc::Bootloader);

  // prevent deletion of this thing
  boot_task->inc_ref();

  Thread_object *boot_thread = new (*Ram_quota::root) Thread_object(*Ram_quota::root);

  assert (boot_thread);

  // prevent deletion of this thing
  boot_thread->inc_ref();

  check (map(boot_task,   boot_task, boot_task, C_task, 0));
  check (map(boot_thread, boot_task, boot_task, C_thread, 0));

  check (boot_thread->control(Thread_ptr(C_pager), Thread_ptr(Thread_ptr::Null)) == 0);
  check (boot_thread->bind(boot_task, User<Utcb>::Ptr((Utcb*)Virt_addr::val(root_utcb.mem_address()))));
  check (boot_thread->ex_regs(Kip::k()->root_ip, 0));

  Ipc_gate *s0_b_gate = Ipc_gate::create(*Ram_quota::root, sigma0_thread, 4 << 4);

  check (s0_b_gate);
  check (map(s0_b_gate, boot_task, boot_task, C_pager, 0));

  sigma0_thread->set_home_cpu(Cpu_number::boot_cpu());
  boot_thread->set_home_cpu(Cpu_number::boot_cpu());

  sigma0_thread->activate();
  check (obj_map(sigma0, C_factory, 1, boot_task, C_factory, 0).error() == 0);
  for (Cap_index c = Initial_kobjects::first(); c < Initial_kobjects::end(); ++c)
    {
      Kobject_iface *o = initial_kobjects->obj(c);
      if (o)
	check(obj_map(sigma0, c, 1, boot_task, c, 0).error() == 0);
    }

  boot_thread->activate();
}
