/*
 * RISC-V Kernel-Info Page
 */

INTERFACE [riscv]:

#include "config.h"
#include "types.h"

EXTENSION class Kip
{
public:
  struct Platform_info
  {
    char name[16];
    Unsigned32 is_mp;
    struct
    {
      // Supported RISC-V ISA extensions
      Unsigned32 isa_ext;
      // Base frequency of the system timer / real-time counter (TIME csr)
      Unsigned32 timebase_frequency;
      // Number of populated entries in the following Hart ID list
      Unsigned32 hart_num;
      // List of harts present in the system, which is used to start secondary
      // harts and to find the interrupt target context corresponding to a
      // hart.
      Unsigned32 hart_ids[16];
      // Base address of the Platform Interrupt Controller (PLIC)
      Mword      plic_addr;
      // Number of IRQs supported by the PLIC
      Unsigned32 plic_nr_irqs;
      // Mapping of harts to their respective PLIC interrupt target context
      Unsigned32 plic_hart_irq_targets[16];
      static_assert(Config::Max_num_cpus <=
        (sizeof(plic_hart_irq_targets) / sizeof(Unsigned32)),
        "Maximum number of CPUs exceeds the PLIC target context map size.");
    } arch;
  };

  /* 0xF0   0x1E0 */
  Platform_info platform_info;
  Unsigned8 __pad[(16 - (sizeof(Platform_info) % 16)) % 16];
};

//---------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

PUBLIC inline
int
Kip::hart_idx(Mword hart_id)
{
  for (unsigned i = 0; i < platform_info.arch.hart_num; i++)
    {
      if (platform_info.arch.hart_ids[i] == hart_id)
        return i;
    }

  return -1;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [riscv && debug]:

IMPLEMENT inline
void
Kip::debug_print_syscalls() const
{}
