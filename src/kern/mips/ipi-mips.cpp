INTERFACE [mips && mp]:

#include "ipi_control.h"

EXTENSION class Ipi
{
public:
  enum Message { Request, Global_request, Debug, Num_requests };

  static Ipi_control *hw;

  Mword atomic_reset(Message m)
  {
    Mword tmp;
    asm volatile (
        ".set push; .set noreorder; .set noat \n"
        "1: move  $at, $zero       \n"
        "   ll    %[tmp], %[ptr]   \n"
        "   sc    $at, %[ptr]      \n"
        "   beqz  $at, 1b          \n"
        "     nop                  \n"
        "   sync                   \n"
        ".set pop                  \n"
        : [tmp] "=&r" (tmp), [ptr] "+m"(_rq[m]));
    return tmp;
  }

  static Ipi *ipis(Cpu_number cpu) { return &_ipi.cpu(cpu); }

  static Mword atomic_reset(Cpu_number cpu, Message m)
  { return ipis(cpu)->atomic_reset(m); }

private:
  Mword _rq[Num_requests];
  Cpu_phys_id _phys_id;
};

// -------------------------------------------------------
IMPLEMENTATION [mips && mp]:

#include "processor.h"
#include "cpu.h"

Ipi_control *Ipi::hw;

IMPLEMENT
void
Ipi::init(Cpu_number cpu)
{
  auto &ipi = _ipi.cpu(cpu);
  for (auto &r: ipi._rq)
    r = 0;

  ipi._phys_id = Proc::cpu_id();
}

PUBLIC static inline
void
Ipi::send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
{
  auto &ipi = _ipi.cpu(to_cpu);
  if (access_once(&ipi._rq[m]))
    return;

  write_now(&ipi._rq[m], true);
  asm volatile ("sync" : : : "memory");

  if (access_once(&ipi._rq[m]))
    hw->send_ipi(to_cpu, &ipi);

  stat_sent(from_cpu);
}

PUBLIC static inline
void
Ipi::eoi(Message, Cpu_number on_cpu)
{
  stat_received(on_cpu);
}

PUBLIC static
void
Ipi::bcast(Message m, Cpu_number from_cpu)
{
  (void)from_cpu;
  Cpu_mask ipis;
  Cpu_number max = Cpu_number::first();
  for (Cpu_number n = Cpu_number::first(); n < Config::max_num_cpus(); ++n)
    {
      if (! Cpu::online(n))
        continue;

      auto &ipi = _ipi.cpu(n);
      if (access_once(&ipi._rq[m]))
        continue;

      write_now(&ipi._rq[m], true);
      ipis.set(n);
      max = n;
    }

  asm volatile ("sync" : : : "memory");

  for (Cpu_number n = Cpu_number::first(); n < max; ++n)
    {
      if (!ipis.get(n))
        continue;

      auto &ipi = _ipi.cpu(n);
      if (access_once(&ipi._rq[m]))
        hw->send_ipi(n, &ipi);
    }
}

// -------------------------------------------------------
IMPLEMENTATION [mips && !mp]:

PUBLIC static
Mword
Ipi::atomic_reset(Cpu_number, Message)
{ return 0; }

