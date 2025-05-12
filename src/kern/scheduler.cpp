INTERFACE:

#include "context.h"
#include "icu_helper.h"
#include "types.h"
#include "global_data.h"

class Scheduler : public Icu_h<Scheduler>, public Irq_chip_virt<1>
{
  friend class Scheduler_test;

  typedef Icu_h<Scheduler> Icu;

public:
  enum class Op : Mword
  {
    Info       = 0,
    Run_thread = 1,
    Idle_time  = 2,
  };

  static Global_data<Scheduler> scheduler;

private:
  L4_RPC(Op::Info,      sched_info, (L4_cpu_set_descr set, Mword *rm,
                                     Mword *max_cpus, Mword *sched_classes));
  L4_RPC(Op::Idle_time, sched_idle, (L4_cpu_set cpus, Cpu_time *time));
};

// ----------------------------------------------------------------------------
IMPLEMENTATION:

#include "thread_object.h"
#include "l4_buf_iter.h"
#include "l4_types.h"
#include "entry_frame.h"
#include "sched_context.h"


JDB_DEFINE_TYPENAME(Scheduler, "\033[34mSched\033[m");
DEFINE_GLOBAL Global_data<Scheduler> Scheduler::scheduler;

PUBLIC void
Scheduler::operator delete (void *)
{
  printf("WARNING: tried to delete kernel scheduler object.\n"
         "         The system is now useless\n");
}

PUBLIC inline
Scheduler::Scheduler()
{
  initial_kobjects->register_obj(this, Initial_kobjects::Scheduler);
}


/*
 * L4-IFACE: kernel-scheduler.scheduler-run_thread
 * PROTOCOL: L4_PROTO_SCHEDULER
 */
PRIVATE
L4_msg_tag
Scheduler::sys_run(L4_fpage::Rights, Syscall_frame *f, Utcb const *utcb)
{
  L4_msg_tag tag = f->tag();
  Cpu_number const curr_cpu = current_cpu();

  unsigned long sz = tag.words() * sizeof(Mword);
  if (EXPECT_FALSE(sz < sizeof(L4_sched_param) + sizeof(Mword)))
    return commit_result(-L4_err::EInval);
  sz -= sizeof(Mword); // skip first Mword containing the Opcode

  Ko::Rights rights;
  Thread *thread = Ko::deref<Thread>(&tag, utcb, &rights);
  if (!thread)
    return tag;

  Mword _store[(Sched_context::max_param_size() + sizeof(Mword) - 1) / sizeof(Mword)];
  memcpy(_store, &utcb->values[1], min<unsigned>(sz, sizeof(_store)));

  auto *sched_param = reinterpret_cast<L4_sched_param const *>(_store);

  if (!sched_param->is_legacy())
    if (EXPECT_FALSE(sched_param->length > sz))
      return commit_result(-L4_err::EInval);

  static_assert(sizeof(L4_sched_param_legacy) <= sizeof(L4_sched_param),
                "Adapt above check");

  int ret = Sched_context::check_param(sched_param);
  if (EXPECT_FALSE(ret < 0))
    return commit_result(ret);

  Thread::Migration info;

  Cpu_number const t_cpu = thread->home_cpu();

  if (Cpu::online(t_cpu) && sched_param->cpus.contains(t_cpu))
    info.cpu = t_cpu;
  else if (sched_param->cpus.contains(curr_cpu))
    info.cpu = curr_cpu;
  else
    info.cpu = sched_param->cpus.first(Cpu::present_mask(), Config::max_num_cpus());

  info.sp = sched_param;
  if constexpr (0) // Intentionally disabled, only used for diagnostics
    printf("CPU[%u]: run(thread=%lx, cpu=%u (%lx,%u,%u)\n",
           cxx::int_value<Cpu_number>(curr_cpu), thread->dbg_id(),
           cxx::int_value<Cpu_number>(info.cpu),
           utcb->values[2],
           cxx::int_value<Cpu_number>(sched_param->cpus.offset()),
           cxx::int_value<Order>(sched_param->cpus.granularity()));

  thread->migrate(&info);

  return commit_result(0);
}

/*
 * L4-IFACE: kernel-scheduler.scheduler-idle_time
 * PROTOCOL: L4_PROTO_SCHEDULER
 */
PRIVATE
L4_msg_tag
Scheduler::op_sched_idle(L4_cpu_set const &cpus, Cpu_time *time)
{
  Cpu_number const cpu = cpus.first(Cpu::online_mask(), Config::max_num_cpus());
  if (EXPECT_FALSE(cpu == Config::max_num_cpus()))
    return commit_result(-L4_err::EInval);

  *time = Context::kernel_context(cpu)->consumed_time();
  return commit_result(0);
}

/*
 * L4-IFACE: kernel-scheduler.scheduler-info
 * PROTOCOL: L4_PROTO_SCHEDULER
 */
PRIVATE
L4_msg_tag
Scheduler::op_sched_info(L4_cpu_set_descr const &s, Mword *m, Mword *max_cpus,
                         Mword *sched_classes)
{
  Mword rm = 0;
  Cpu_number max = Config::max_num_cpus();
  Order granularity = s.granularity();
  Cpu_number const offset = s.offset();

  if (offset >= max)
    return commit_result(-L4_err::ERange);

  if (max > offset + Cpu_number(MWORD_BITS) << granularity)
    max = offset + Cpu_number(MWORD_BITS) << granularity;

  for (Cpu_number i = Cpu_number::first(); i < max - offset; ++i)
    if (Cpu::present_mask().get(i + offset))
      rm |= 1ul << cxx::int_value<Cpu_number>(i >> granularity);

  *m = rm;
  *max_cpus = Config::Max_num_cpus;
  *sched_classes = Sched_context::sched_classes();

  return commit_result(0);
}

PUBLIC inline
void
Scheduler::trigger_hotplug_event()
{
  if (auto i = icu_irq(0))
    i->hit(nullptr);
}

PUBLIC
L4_msg_tag
Scheduler::kinvoke(L4_obj_ref ref, L4_fpage::Rights rights, Syscall_frame *f,
                   Utcb const *iutcb, Utcb *outcb)
{
  L4_msg_tag tag = f->tag();

  if (tag.proto() == L4_msg_tag::Label_irq)
    return Icu::icu_invoke(ref, rights, f, iutcb, outcb);

  if (!Ko::check_basics(&tag, L4_msg_tag::Label_scheduler))
    return tag;

  switch (Op{iutcb->values[0]})
    {
    case Op::Info:       return Msg_sched_info::call(this, tag, iutcb, outcb);
    case Op::Run_thread: return sys_run(rights, f, iutcb);
    case Op::Idle_time:  return Msg_sched_idle::call(this, tag, iutcb, outcb);
    default:             return commit_result(-L4_err::ENosys);
    }
}
