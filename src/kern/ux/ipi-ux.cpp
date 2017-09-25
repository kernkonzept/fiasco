INTERFACE [mp]:

EXTENSION class Ipi
{
private:
  Unsigned32 _lcpu;

public:
  enum Message { Request = 'r', Global_request = 'g', Debug = 'd' };
};

//---------------------------------------------------------------------------
IMPLEMENTATION[!mp]:

//---------------------------------------------------------------------------
IMPLEMENTATION[mp]:

#include <cstdio>

#include "cpu.h"
#include "pic.h"

PUBLIC inline
Ipi::Ipi() : _lcpu(~0)
{}

IMPLEMENT
void
Ipi::init()
{
  _lcpu = current_cpu();
  Pic::setup_ipi(_lcpu, Cpu::cpus.cpu(_lcpu).phys_id());
}

PUBLIC static inline NEEDS[<cstdio>]
void
Ipi::eoi(Message)
{
}

PUBLIC static inline NEEDS[<cstdio>, "pic.h"]
void
Ipi::send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
{
  printf("Sending IPI:%d to CPU%d\n", m, _lcpu);
  Pic::send_ipi(_lcpu, m);
}

PUBLIC static inline NEEDS[<cstdio>, "cpu.h", "pic.h"]
void
Ipi::bcast(Message m, Cpu_number from_cpu)
{
  printf("Bcast IPI:%d\n", m);
  for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    if (Cpu::online(i))
      Pic::send_ipi(i, m);
}

PUBLIC static
unsigned long
Ipi::gate(unsigned char data)
{
  Message m = (Message)data;

  switch (m)
    {
      case Global_request: return APIC_IRQ_BASE + 2;
      case Request:        return APIC_IRQ_BASE - 1;
      case Debug:          return APIC_IRQ_BASE - 2;
      default:
         printf("Unknown request: %c(%d)\n", m, m);
         break;
    }
  return ~0UL;
}
