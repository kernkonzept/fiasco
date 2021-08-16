IMPLEMENTATION[riscv]:

#include "platform_control.h"
#include "processor.h"

void __attribute__ ((noreturn))
platform_reset()
{
  Platform_control::system_reboot();

  for (;;)
    Proc::halt();
}
