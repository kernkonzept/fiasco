INTERFACE [riscv && mp]:

#include "atomic.h"

EXTENSION class Ipi
{
public:
  enum Message : Mword {
    Request        = 1 << 0,
    Global_request = 1 << 1,
    Debug          = 1 << 2,
  };

  static Mword pending_ipis_reset(Cpu_number cpu)
  {
    return atomic_mp_xchg(&_ipi.cpu(cpu)._rq, 0);
  }

private:
  bool atomic_set(Message m)
  {
    return !(atomic_mp_or(&_rq, m) & m);
  }

private:
  Mword _rq;
  Cpu_phys_id _phys_id;
};

// ---------------------------------------------------------------------------
IMPLEMENTATION [riscv && mp]:

#include "cpu.h"
#include "sbi.h"

PUBLIC inline
Ipi::Ipi()
{}


IMPLEMENT inline
void
Ipi::init(Cpu_number cpu)
{
  auto &ipi = _ipi.cpu(cpu);
  ipi._rq = 0;
  ipi._phys_id = Cpu::cpus.cpu(cpu).phys_id();
}

PUBLIC static inline
void
Ipi::eoi(Message, Cpu_number on_cpu)
{
  stat_received(on_cpu);
}

PUBLIC static inline NEEDS["cpu.h", Ipi::send_ipi]
void
Ipi::send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
{
  auto &ipi = _ipi.cpu(to_cpu);
  if (!ipi.atomic_set(m))
    return;

  send_ipi(Hart_mask(ipi._phys_id));

  stat_sent(from_cpu);
}

PUBLIC static inline NEEDS["cpu.h", Ipi::send_ipi]
void
Ipi::bcast(Message m, Cpu_number from_cpu)
{
  (void)from_cpu;

  Hart_mask hart_mask;
  for (Cpu_number n = Cpu_number::first(); n < Config::max_num_cpus(); ++n)
    {
      if (!Cpu::online(n))
        continue;

      auto &ipi = _ipi.cpu(n);
      if (ipi.atomic_set(m))
        hart_mask.set(ipi._phys_id);
    }

  send_ipi(hart_mask);
}

PRIVATE static inline NEEDS["sbi.h"]
void
Ipi::send_ipi(Mword hart_mask)
{
  Sbi::send_ipi(hart_mask);
}

// -------------------------------------------------------
IMPLEMENTATION [riscv && !mp]:

PUBLIC static
Mword
Ipi::atomic_reset(Cpu_number)
{ return 0; }
