IMPLEMENTATION[riscv]:

#include "processor.h"
#include "sbi.h"

[[noreturn]] void
platform_reset()
{
  if (Sbi::Srst::probe())
    Sbi::Srst::system_reset(Sbi::Srst::Type_cold_reboot,
                            Sbi::Srst::Reason_no_reason);

  for (;;)
    Proc::halt();
}
