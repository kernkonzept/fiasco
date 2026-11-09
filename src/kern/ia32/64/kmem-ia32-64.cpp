IMPLEMENTATION [amd64]:

#include "panic.h"
#include "tss.h"

EXTENSION class Kmem
{
  enum { Ist_stack_size = 256 };
};

/**
 * Allocate an interrupt stack and set it for an interrupt stack table entry.
 *
 * The top of the stack holds a pointer to the kernel stack pointer of the
 * CPU and the CPU number, see IST_ENTER in entry.S and entry_vec08_dbf in
 * entry-native.S.
 */
PRIVATE static FIASCO_INIT_CPU
void
Kmem::init_ist(Cpu &cpu, Lockless_alloc *cpu_alloc, Unsigned8 ist)
{
  Unsigned8 *stack = cpu_alloc->alloc_bytes<Unsigned8>(Ist_stack_size, Order(4));
  if (!stack)
    panic("Cannot allocate interrupt stack");

  struct alignas(16) Ist_stack_info
  {
    Mword kernel_sp;    // Pointer to the kernel stack pointer of this CPU.
    Mword cpu;          // CPU number.
  };

  auto *info = reinterpret_cast<Ist_stack_info *>(stack + Ist_stack_size
                                                  - sizeof(Ist_stack_info));
  info->kernel_sp = reinterpret_cast<Mword>(&cpu.kernel_sp());
  info->cpu = cxx::int_value<Cpu_number>(cpu.id());
  cpu.set_ist(ist, reinterpret_cast<Address>(info));
}

PRIVATE static FIASCO_INIT_CPU
void
Kmem::init_cpu_arch(Cpu &cpu, Lockless_alloc *cpu_alloc)
{
  init_ist(cpu, cpu_alloc, Tss::Ist_double_fault);
  init_ist(cpu, cpu_alloc, Tss::Ist_nmi);
  init_ist(cpu, cpu_alloc, Tss::Ist_machine_check);
}
