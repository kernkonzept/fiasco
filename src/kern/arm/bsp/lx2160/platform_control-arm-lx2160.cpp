// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_lx2160 && arm_psci]:

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  boot_ap_cpus_psci(phys_tramp_mp_addr,
                    {  0x0, 0x001,
                     0x100, 0x101,
                     0x200, 0x201,
                     0x300, 0x301,
                     0x400, 0x401,
                     0x500, 0x501,
                     0x600, 0x601,
                     0x700, 0x701 });
}
