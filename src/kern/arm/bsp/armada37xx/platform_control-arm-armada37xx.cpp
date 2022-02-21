IMPLEMENTATION [arm && pf_armada37xx]:

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  boot_ap_cpus_psci(phys_tramp_mp_addr,
                    { 0x0, 0x1 });
}
