INTERFACE [riscv]:

#include "types.h"

//-------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include <cstdlib>
#include <cstdio>
#include <construction.h>
#include "processor.h"

[[noreturn]] void kernel_main(void);

extern "C"
[[noreturn]] void __main(Mword hart_id)
{
  // Provide initial stack allocated hart context (will be replaced later on)
  Proc::Hart_context hart_context;
  hart_context._phys_id = Cpu_phys_id(hart_id);
  Proc::hart_context(&hart_context);

  static_construction();
  kernel_main();
}
