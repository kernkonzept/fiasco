INTERFACE [arm && mp && (pf_omap4 || pf_omap5)]:

#include "types.h"
#include "mmio_register_block.h"

IMPLEMENTATION [arm && mp && (pf_omap4 || pf_omap5)]: // ------------------------

#include "io.h"
#include "kmem_mmio.h"

PRIVATE static
void
Platform_control::aux(unsigned cmd, Mword arg0, Mword arg1)
{
  register unsigned long r0  asm("r0")  = arg0;
  register unsigned long r1  asm("r1")  = arg1;
  register unsigned long r12 asm("r12") = cmd;

  asm volatile(".arch_extension sec\n"
               "dsb                \n"
#ifdef __thumb__
               "push {r7}          \n"
#else
               "push {r11}         \n"
#endif
               "smc #0             \n"
#ifdef __thumb__
               "pop {r7}           \n"
#else
               "pop {r11}          \n"
#endif
               : : "r" (r0), "r" (r1), "r" (r12)
               : "r2", "r3", "r4", "r5", "r6", "r8", "r9", "r10", "r14",
#ifdef __thumb__
                 "r11",
#else
                 "r7",
#endif
                 "memory");
}

IMPLEMENTATION [arm && mp && pf_omap4]: // -----------------------------------

PRIVATE static
void
Platform_control::setup_ap_boot(Mmio_register_block *aux, unsigned reg)
{
  aux->modify<Mword>(0x200, 0xfffffdff, reg);
}

IMPLEMENTATION [arm && mp && pf_omap5]: // -----------------------------------

PRIVATE static
void
Platform_control::setup_ap_boot(Mmio_register_block *aux, unsigned reg)
{
  aux->write<Mword>(0x20, reg);
}

IMPLEMENTATION [arm && mp && (pf_omap4 || pf_omap5)]: // ------------------------

#include "ipi.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  // omap4: two possibilities available, the memory mapped only in later
  // board revisions
  if (0)
    {
      enum {
        AUX_CORE_BOOT_0 = 0x104,
        AUX_CORE_BOOT_1 = 0x105,
      };
      aux(AUX_CORE_BOOT_1, phys_tramp_mp_addr, 0);
      asm volatile("dsb; sev" : : : "memory");
      aux(AUX_CORE_BOOT_0, 0x200, 0xfffffdff);
    }
  else
    {
      enum {
        AUX_CORE_BOOT_0 = 0,
        AUX_CORE_BOOT_1 = 4,
      };

      Mmio_register_block aux(Kmem_mmio::map(0x48281800, 0x800));
      aux.write<Mword>(phys_tramp_mp_addr, AUX_CORE_BOOT_1);
      setup_ap_boot(&aux, AUX_CORE_BOOT_0);
      asm volatile("dsb; sev" : : : "memory");
      Ipi::bcast(Ipi::Global_request, current_cpu());
    }
}
