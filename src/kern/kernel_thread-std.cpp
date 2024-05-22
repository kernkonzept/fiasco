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
#include "warn.h"

static constexpr Cap_index C_task    = Cap_index(Initial_kobjects::Task);
static constexpr Cap_index C_thread  = Cap_index(Initial_kobjects::Thread);
static constexpr Cap_index C_factory = Cap_index(Initial_kobjects::Factory);
static constexpr Cap_index C_pager   = Cap_index(Initial_kobjects::Pager);

PRIVATE
Task *
Kernel_thread::create_sigma0_task()
{
  int err;
  Task *sigma0 = Task::create<Sigma0_task>(Ram_quota::root, L4_msg_tag(), 0, 0,
                                           &err, 0);
  assert_opt (sigma0);
  // prevent deletion of the sigma0 task
  sigma0->inc_ref();

  sigma0->map_all_segs(Mem_desc::Dedicated);

  init_mapdb_mem(sigma0);
  init_mapdb_io(sigma0);

  check (map_obj_initially(sigma0,          sigma0, sigma0, C_task, 0));
  check (map_obj_initially(Factory::root(), sigma0, sigma0, C_factory, 0));

  for (Cap_index c = Initial_kobjects::first(); c < Initial_kobjects::end(); ++c)
    {
      Kobject_iface *o = initial_kobjects.obj(c);
      if (o)
        check(map_obj_initially(o, sigma0, sigma0, c, 0));
    }
  return sigma0;
}

PRIVATE
Task *
Kernel_thread::create_boot_task(Task *sigma0, Thread *sigma0_thread)
{
  int err;
  Task *boot_task = Task::create<Task>(Ram_quota::root, L4_msg_tag(), 0, 0,
                                       &err, 0);
  assert_opt (boot_task);
  // prevent deletion of the boot task
  boot_task->inc_ref();

  boot_task->map_all_segs(Mem_desc::Bootloader);

  check (map_obj_initially(boot_task,   boot_task, boot_task, C_task, 0));
  check (obj_map(sigma0, C_factory, 1, boot_task, C_factory, 0).error() == 0);

  Ipc_gate *s0_b_gate = Ipc_gate::create(Ram_quota::root, sigma0_thread, 4 << 4);
  check (s0_b_gate);
  check (map_obj_initially(s0_b_gate, boot_task, boot_task, C_pager, 0));

  for (Cap_index c = Initial_kobjects::first(); c < Initial_kobjects::end(); ++c)
    {
      Kobject_iface *o = initial_kobjects.obj(c);
      if (o)
        check(obj_map(sigma0, c, 1, boot_task, c, 0).error() == 0);
    }
  return boot_task;
}

PRIVATE
Thread_object *
Kernel_thread::create_user_thread(Task *task, Thread_ptr const &pager, Address ip)
{
  Thread_object *thread = new (Ram_quota::root) Thread_object(Ram_quota::root);
  assert_opt(thread);
  // prevent deletion of this thing
  thread->inc_ref();

  check (map_obj_initially(thread, task, task, C_thread, 0));

  // Task just newly created, no need for locking or remote TLB flush.
  L4_fpage utcb_fp = L4_fpage::mem(utcb_addr(), Config::PAGE_SHIFT);
  check(task->alloc_ku_mem(&utcb_fp, false) >= 0);

  check (thread->control(pager, Thread_ptr(Thread_ptr::Null)) == 0);
  auto utcb_usr_ptr = User_ptr<Utcb>(
    reinterpret_cast<Utcb*>(cxx::int_value<Virt_addr>(utcb_fp.mem_address())));
  check (thread->bind(task, utcb_usr_ptr));
  check (thread->ex_regs(ip, 0));

  thread->set_home_cpu(Cpu_number::boot_cpu());
  return thread;
}

IMPLEMENT
void
Kernel_thread::init_workload()
{
  if (!Kip::k()->sigma0_ip)
    {
      WARN("No sigma0! Check your setup.\n");
      return;
    }

  auto g = lock_guard(cpu_lock);

  // create sigma0
  Task *sigma0 = create_sigma0_task();
  Thread_object *sigma0_thread =
    create_user_thread(sigma0, Thread_ptr(Thread_ptr::Null), Kip::k()->sigma0_ip);
  sigma0_thread->activate();

  if (!Kip::k()->root_ip)
    return;

  // create the boot task
  Task *boot_task = create_boot_task(sigma0, sigma0_thread);
  Thread_object *boot_thread =
    create_user_thread(boot_task, Thread_ptr(C_pager), Kip::k()->root_ip);
  boot_thread->activate();
}
