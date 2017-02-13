INTERFACE [arm && pf_bcm283x && mp]: // -----------------------------------

EXTENSION class Ipi
{
public:
  enum Message { Global_request, Request, Debug, Timer };

  static Message pending();

private:
  Cpu_phys_id _phys_id;
};

IMPLEMENTATION [arm && pf_bcm283x && mp]: // ------------------------------

#include "arm_control.h"
#include "cpu.h"

IMPLEMENT
void
Ipi::init(Cpu_number cpu)
{
  _ipi.cpu(cpu)._phys_id = Proc::cpu_id();
  Arm_control::o()->mailbox_unmask(0, Proc::cpu_id());
}

PUBLIC static
void
Ipi::send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
{
  Arm_control::o()->send_ipi(m, _ipi.cpu(to_cpu)._phys_id);
  stat_sent(from_cpu);
}

PUBLIC static
void
Ipi::send(Message m, Cpu_number from_cpu, Cpu_phys_id to_cpu)
{
  Arm_control::o()->send_ipi(m, to_cpu);
  stat_sent(from_cpu);
}

PUBLIC static
void
Ipi::bcast(Message m, Cpu_number from_cpu)
{
  for (Cpu_number n = Cpu_number::first(); n < Config::max_num_cpus(); ++n)
    if (Cpu::online(n) && n != from_cpu)
      send(m, from_cpu, n);
}

PUBLIC static inline
void Ipi::eoi(Message, Cpu_number on_cpu)
{
  stat_received(on_cpu);
}

IMPLEMENT
Ipi::Message
Ipi::pending()
{
  return (Ipi::Message)Arm_control::o()->ipi_pending();
}
