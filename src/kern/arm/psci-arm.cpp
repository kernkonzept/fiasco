INTERFACE [arm]:

#include <types.h>

class Psci
{
public:
  static void init(Cpu_number cpu);
};

// ------------------------------------------------------------------------
INTERFACE [arm && arm_psci]:

EXTENSION class Psci
{
public:
  struct Result
  {
    Mword res[4];
  };

  enum Error_codes
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

private:
  enum Functions
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

  static bool is_v1;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_psci]:

IMPLEMENT
void
Psci::init(Cpu_number)
{}

// ------------------------------------------------------------------------
INTERFACE [arm && arm_psci && arm_psci_smc]:

#define FIASCO_ARM_PSCI_CALL_ASM_FUNC "smc #0"

// ------------------------------------------------------------------------
INTERFACE [arm && arm_psci && arm_psci_hvc]:

#define FIASCO_ARM_PSCI_CALL_ASM_FUNC "hvc #0"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_psci]:

#include "mem.h"
#include "mmio_register_block.h"
#include "kmem.h"
#include "smc_call.h"

#include <cstdio>

bool Psci::is_v1;

PRIVATE static
unsigned long
Psci::psci_fn(unsigned fn)
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

PUBLIC static inline NEEDS ["smc_call.h"]
Psci::Result
Psci::psci_call(Mword fn_id,
                Mword a0 = 0, Mword a1 = 0,
                Mword a2 = 0, Mword a3 = 0,
                Mword a4 = 0, Mword a5 = 0,
                Mword a6 = 0)
{
  register Mword r0 FIASCO_ARM_ASM_REG(0) = psci_fn(fn_id);
  register Mword r1 FIASCO_ARM_ASM_REG(1) = a0;
  register Mword r2 FIASCO_ARM_ASM_REG(2) = a1;
  register Mword r3 FIASCO_ARM_ASM_REG(3) = a2;
  register Mword r4 FIASCO_ARM_ASM_REG(4) = a3;
  register Mword r5 FIASCO_ARM_ASM_REG(5) = a4;
  register Mword r6 FIASCO_ARM_ASM_REG(6) = a5;
  register Mword r7 FIASCO_ARM_ASM_REG(7) = a6;

  asm volatile(FIASCO_ARM_PSCI_CALL_ASM_FUNC
               FIASCO_ARM_SMC_CALL_ASM_OPERANDS);

  Result res = { r0, r1, r2, r3 };
  return res;
}

IMPLEMENT
void
Psci::init(Cpu_number cpu)
{
  if (cpu != Cpu_number::boot_cpu())
    return;

  printf("Detecting PSCI ...\n");
  Result r = psci_call(Psci_version);
  printf("Detected PSCI v%ld.%ld\n", r.res[0] >> 16, r.res[0] & 0xffff);

  is_v1 = (r.res[0] >> 16) >= 1;

  if (is_v1)
    {
      r = psci_call(Psci_features, psci_fn(Psci_cpu_suspend));
      if (r.res[0] & (1UL << 31))
        printf("PSCI: CPU_Suspend not supported (%d)\n", (int)r.res[0]);
      else
        printf("PSCI: CPU_SUSPEND format %s, %s OS-initiated mode\n",
               r.res[0] & 2 ? "extended" : "original v0.2",
               r.res[0] & 1 ? "supports" : "does not support");
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
Psci::cpu_on(unsigned long target, Address phys_tramp_mp_addr)
{
  Result r = psci_call(Psci_cpu_on, target, phys_tramp_mp_addr);
  return r.res[0];
}

PUBLIC static
void
Psci::system_reset()
{
  psci_call(Psci_system_reset);
  printf("PSCI reset failed.\n");
}

PUBLIC static
void
Psci::system_off()
{
  psci_call(Psci_system_off);
  printf("PSCI system-off failed.\n");
}
