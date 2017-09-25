//-----------------------------------------------------------------------------
IMPLEMENTATION [!mp && irregular_gic]:

PUBLIC static inline
void
Thread::init_arm_ipis(Cpu_number)
{}

//-----------------------------------------------------------------------------
IMPLEMENTATION [mp && irregular_gic]:

#include "pic.h"

class Arm_ipis
{
public:
  Arm_ipis(Cpu_number cpu, bool resume)
  {
    if (!resume)
      {
        check(Pic::gic.cpu(cpu)->alloc(&remote_rq_ipi, Ipi::Request));
        check(Pic::gic.cpu(cpu)->alloc(&glbl_remote_rq_ipi, Ipi::Global_request));
        check(Pic::gic.cpu(cpu)->alloc(&debug_ipi, Ipi::Debug));
        check(Pic::gic.cpu(cpu)->alloc(&timer_ipi, Ipi::Timer));
      }
  }

  Thread_remote_rq_irq remote_rq_ipi;
  Thread_glbl_remote_rq_irq glbl_remote_rq_ipi;
  Thread_debug_ipi debug_ipi;
  Thread_timer_tick_ipi timer_ipi;
};

DEFINE_PER_CPU static Per_cpu<Static_object<Arm_ipis> > _arm_ipis;

IMPLEMENT_OVERRIDE
void
Thread::init_per_cpu(Cpu_number cpu, bool resume)
{
  _arm_ipis.cpu(cpu).construct(cpu, resume);
}
