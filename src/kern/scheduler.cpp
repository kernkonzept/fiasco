INTERFACE:

#include "context.h"
#include "icu_helper.h"
#include "types.h"

class Scheduler : public Icu_h<Scheduler>, public Irq_chip_soft
{
  typedef Icu_h<Scheduler> Icu;

public:
  enum Operation
  {
    Info       = 0,
    Run_thread = 1,
    Idle_time  = 2,
  };

  static Scheduler scheduler;
private:
  Irq_base *_irq;

  L4_RPC(Info,      sched_info, (L4_cpu_set_descr set, Mword *rm, Mword *max_cpus));
  L4_RPC(Idle_time, sched_idle, (L4_cpu_set cpus, Cpu_time *time));
};

// ----------------------------------------------------------------------------
IMPLEMENTATION:

#include "thread_object.h"
#include "l4_buf_iter.h"
#include "l4_types.h"
#include "entry_frame.h"


JDB_DEFINE_TYPENAME(Scheduler, "\033[34mSched\033[m");
Scheduler Scheduler::scheduler;

PUBLIC void
Scheduler::operator delete (void *)
{
  printf("WARNING: tried to delete kernel scheduler object.\n"
         "         The system is now useless\n");
}

PUBLIC inline
Scheduler::Scheduler() : _irq(0)
{
  initial_kobjects.register_obj(this, Initial_kobjects::Scheduler);
}


PRIVATE
L4_msg_tag
Scheduler::sys_run(L4_fpage::Rights, Syscall_frame *f, Utcb const *utcb)
{
  L4_msg_tag tag = f->tag();
  Cpu_number const curr_cpu = current_cpu();

  if (EXPECT_FALSE(tag.words() < 5))
    return commit_result(-L4_err::EInval);

  unsigned long sz = sizeof (L4_sched_param_legacy);

    {
      L4_sched_param const *sched_param = reinterpret_cast<L4_sched_param const*>(&utcb->values[1]);
      if (sched_param->sched_class < 0)
        sz = sched_param->length;

      sz += sizeof(Mword) - 1;
      sz /= sizeof(Mword);

      if (sz + 1 > tag.words())
	return commit_result(-L4_err::EInval);
    }

  Ko::Rights rights;
  Thread *thread = Ko::deref<Thread>(&tag, utcb, &rights);
  if (!thread)
    return tag;


  Mword _store[sz];
  memcpy(_store, &utcb->values[1], sz * sizeof(Mword));

  L4_sched_param const *sched_param = reinterpret_cast<L4_sched_param const *>(_store);

  Thread::Migration info;

  Cpu_number const t_cpu = thread->home_cpu();

  if (Cpu::online(t_cpu) && sched_param->cpus.contains(t_cpu))
    info.cpu = t_cpu;
  else if (sched_param->cpus.contains(curr_cpu))
    info.cpu = curr_cpu;
  else
    info.cpu = sched_param->cpus.first(Cpu::present_mask(), Config::max_num_cpus());

  info.sp = sched_param;
  if (0)
    printf("CPU[%u]: run(thread=%lx, cpu=%u (%lx,%u,%u)\n",
           cxx::int_value<Cpu_number>(curr_cpu), thread->dbg_id(),
           cxx::int_value<Cpu_number>(info.cpu),
           utcb->values[2],
           cxx::int_value<Cpu_number>(sched_param->cpus.offset()),
           cxx::int_value<Order>(sched_param->cpus.granularity()));

  thread->migrate(&info);

  return commit_result(0);
}

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

PRIVATE
L4_msg_tag
Scheduler::op_sched_info(L4_cpu_set_descr const &s, Mword *m, Mword *max_cpus)
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
      rm |= (1 << cxx::int_value<Cpu_number>(i >> granularity));

  *m = rm;
  *max_cpus = Config::Max_num_cpus;
  return commit_result(0);
}

PUBLIC inline
Irq_base *
Scheduler::icu_get_irq(unsigned irqnum)
{
  if (irqnum > 0)
    return 0;

  return _irq;
}

PUBLIC inline
L4_msg_tag
Scheduler::op_icu_get_info(Mword *features, Mword *num_irqs, Mword *num_msis)
{
  *features = 0; // supported features (only normal irqs)
  *num_irqs = 1;
  *num_msis = 0;
  return L4_msg_tag(0);
}

PUBLIC
L4_msg_tag
Scheduler::op_icu_bind(unsigned irqnum, Ko::Cap<Irq> const &irq)
{
  if (irqnum > 0)
    return commit_result(-L4_err::EInval);

  if (_irq)
    _irq->unbind();

  if (!Ko::check_rights(irq.rights, Ko::Rights::CW()))
    return commit_result(-L4_err::EPerm);

  Irq_chip_soft::bind(irq.obj, irqnum);
  _irq = irq.obj;
  return commit_result(0);
}

PUBLIC
L4_msg_tag
Scheduler::op_icu_set_mode(Mword pin, Irq_chip::Mode)
{
  if (pin != 0)
    return commit_result(-L4_err::EInval);

  if (_irq)
    _irq->switch_mode(true);
  return commit_result(0);
}

PUBLIC inline
void
Scheduler::trigger_hotplug_event()
{
  if (_irq)
    _irq->hit(0);
}

PUBLIC
L4_msg_tag
Scheduler::kinvoke(L4_obj_ref ref, L4_fpage::Rights rights, Syscall_frame *f,
                   Utcb const *iutcb, Utcb *outcb)
{
  switch (f->tag().proto())
    {
    case L4_msg_tag::Label_irq:
      return Icu::icu_invoke(ref, rights, f, iutcb,outcb);
    case L4_msg_tag::Label_scheduler:
      break;
    default:
      return commit_result(-L4_err::EBadproto);
    }

  switch (iutcb->values[0])
    {
    case Info:       return Msg_sched_info::call(this, f->tag(), iutcb, outcb);
    case Run_thread: return sys_run(rights, f, iutcb);
    case Idle_time:  return Msg_sched_idle::call(this, f->tag(), iutcb, outcb);
    default:         return commit_result(-L4_err::ENosys);
    }
}
