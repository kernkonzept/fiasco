INTERFACE [arm && pf_fvp_base]: // ----------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_fvp {
    Gic_dist_phys_base   = 0x2f000000,
    Gic_redist_phys_base = 0x2f100000,
    Gic_redist_size      = 0x00200000,
    Gic_its_phys_base    = 0x2f020000,
    Gic_its_size         = 0x00020000,

    // VE System register device, necessary for shutdown
    Ve_sysregs_base   = 0x1c010000,
    Ve_sysregs_size   = 0x10000,
  };
};
