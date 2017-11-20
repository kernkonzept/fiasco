INTERFACE [arm && arm_psci]:

#include <types.h>

EXTENSION class Platform_control
{
public:
  struct Psci_result
  {
    Mword res[4];
  };


private:
  enum Psci_error_codes
  {
    Psci_success            =  0,
    Psci_not_supported      = -1,
    Psci_invalid_parameters = -2,
    Psci_denied             = -3,
    Psci_already_on         = -4,
    Psci_on_pending         = -5,
    Psci_internal_failure   = -6,
    Psci_not_present        = -7,
    Psci_disabled           = -8,
    Psci_invalid_address    = -9,
  };

  enum Psci_functions
  {
    Psci_base_smc32          = 0x84000000,
    Psci_base_smc64          = 0xC4000000,

    Psci_version             = 0,
    Psci_cpu_suspend         = 1,
    Psci_cpu_off             = 2,
    Psci_cpu_on              = 3,
    Psci_affinity_info       = 4,
    Psci_migrate             = 5,
    Psci_migrate_info_type   = 6,
    Psci_migrate_info_up_cpu = 7,
    Psci_system_off          = 8,
    Psci_system_reset        = 9,
    Psci_features            = 10,
    Psci_cpu_freeze          = 11,
    Psci_cpu_default_suspend = 12,
    Psci_node_hw_state       = 13,
    Psci_system_suspend      = 14,
    Psci_set_suspend_mode    = 15,
    Psci_stat_residency      = 16,
    Psci_stat_count          = 17,
  };

  static bool psci_is_v1;
};

IMPLEMENTATION [arm && arm_psci]:

#include "mem.h"
#include "mmio_register_block.h"
#include "kmem.h"

#include <cstdio>

bool Platform_control::psci_is_v1;

PRIVATE static
unsigned long
Platform_control::psci_fn(unsigned fn)
{
  switch (fn)
    {
    case Psci_version:
    case Psci_cpu_off:
    case Psci_migrate_info_type:
    case Psci_system_off:
    case Psci_system_reset:
    case Psci_features:
    case Psci_cpu_freeze:
    case Psci_set_suspend_mode:
      return Psci_base_smc32 + fn;
    default:
      return (sizeof(long) == 8 ? Psci_base_smc64 : Psci_base_smc32) + fn;
    };
}

PUBLIC static inline
Platform_control::Psci_result
Platform_control::psci_call(Mword fn_id,
                            Mword a0 = 0, Mword a1 = 0,
                            Mword a2 = 0, Mword a3 = 0,
                            Mword a4 = 0, Mword a5 = 0,
                            Mword a6 = 0)
{
  register Mword r0 asm("r0") = psci_fn(fn_id);
  register Mword r1 asm("r1") = a0;
  register Mword r2 asm("r2") = a1;
  register Mword r3 asm("r3") = a2;
  register Mword r4 asm("r4") = a3;
  register Mword r5 asm("r5") = a4;
  register Mword r6 asm("r6") = a5;
  register Mword r7 asm("r7") = a6;

  asm volatile("smc #0"
              : "=r" (r0), "=r" (r1), "=r" (r2), "=r" (r3)
              : "0" (r0), "1" (r1), "2" (r2), "3" (r3),
                "r" (r4), "r" (r5), "r" (r6), "r" (r7)
              : "memory");

  return { r0, r1, r2, r3 };
}

IMPLEMENT_OVERRIDE
void
Platform_control::init(Cpu_number cpu)
{
  if (cpu != Cpu_number::boot_cpu())
    return;

  Psci_result r = psci_call(Psci_version);
  printf("Detected PSCI v%ld.%ld\n", r.res[0] >> 16, r.res[0] & 0xffff);

  psci_is_v1 = (r.res[0] >> 16) >= 1;

  if (psci_is_v1)
    {
      r = psci_call(Psci_features, psci_fn(Psci_cpu_suspend));
      if (r.res[0] & (1UL << 31))
        printf("PSCI: CPU_Suspend not supported (%d)\n", (int)r.res[0]);
      else
        printf("PSCI: CPU_SUSPEND format %s, %ssupports OS initiated mode\n",
               r.res[0] & 2 ? "extended" : "original v0.2",
               r.res[0] & 1 ? "" : "not ");
    }

  r = psci_call(Psci_migrate_info_type);

  if (r.res[0] == 0 || r.res[0] == 1)
    printf("PSCI: TOS: single core, %smigration capable.\n",
           r.res[0] ? "not " : "");
  else
    printf("PSCI: TOS: Not present or not required.\n");
}

PUBLIC static
int
Platform_control::cpu_on(unsigned long target, Address phys_tramp_mp_addr)
{
  Psci_result r = psci_call(Psci_cpu_on, target, phys_tramp_mp_addr);
  return r.res[0];
}

PUBLIC static
void
Platform_control::system_reset()
{
  psci_call(Psci_system_reset);
  printf("PSCI reset failed.\n");
}
