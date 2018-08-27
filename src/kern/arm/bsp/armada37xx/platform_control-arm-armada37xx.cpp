IMPLEMENTATION [arm && pf_armada37xx]:

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  for (int i = 1; i < 2; ++i)
    {
      int r;
      static unsigned const coreid[2] = { 0, 1 };
      if ((r = cpu_on(coreid[i], phys_tramp_mp_addr)))
        printf("KERNEL: PSCI CPU_ON for CPU%d failed: %d\n", i, r);
    }
}
