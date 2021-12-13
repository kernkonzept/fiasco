IMPLEMENTATION [arm && psci && pf_sbsa]:

#include "acpi.h"
#include "acpi_fadt.h"
#include "psci.h"
#include "panic.h"

PRIVATE static
bool
Psci::is_hvc()
{
  static enum { Unk, Smc, Hvc } type;

  // Can be called multiple times. Parse the table only once...
  if (type == Unk)
    {
      auto *fadt = Acpi::find<Acpi_fadt const *>("FACP");
      if (!fadt)
        panic("No FADT!");

      if (!(fadt->flags & Acpi_fadt::Hw_reduced_acpi))
        panic("FADT: no hardware-reduced ACPI");
      if (!(fadt->arm_boot_flags & Acpi_fadt::Psci_compliant))
        panic("FADT: PSCI not supported");

      type = (fadt->arm_boot_flags & Acpi_fadt::Psci_use_hvc) ? Hvc : Smc;
    }

  return type == Hvc;
}
