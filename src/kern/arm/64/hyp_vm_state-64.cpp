INTERFACE:

EXTENSION class Hyp_vm_state
{
public:
  struct Regs_g
  {
    Unsigned64 hcr;

    Unsigned32 sctlr;
    Unsigned32 cntkctl;
    Unsigned32 mdcr;
    Unsigned32 mdscr;
    Unsigned32 cpacr;
    // dummy to keep 64bit alignment and total size
    // of Regs_g + Regs_h pair
    Unsigned32 _dummy_;
  };

  struct Regs_h
  {
    Unsigned64 hcr;

    Unsigned32 dummy;
    Unsigned32 mdscr;
  };

  typedef Gic_h_global::Arm_vgic Gic;

  /* The following part is our user API */
  Regs_g guest_regs;
  Regs_h host_regs;

  Unsigned64 cntvoff;

  Unsigned64 cntv_cval;
  Unsigned32 cntkctl;
  Unsigned32 cntv_ctl;

  Unsigned64 vmpidr;
  Unsigned32 vpidr;

  Unsigned32 _rsvd[3];

  // size depdens on gic version, numer of LRs and APRs
  Gic  gic;

  /* The user API ends here */

  /* we should align this at a cache line ... */
  Unsigned64 hcr;
  Unsigned64 actlr;

  Unsigned64 tcr;
  Unsigned64 ttbr0;
  Unsigned64 ttbr1;

  Unsigned32 csselr;
  Unsigned32 sctlr;
  Unsigned32 cpacr;
  Unsigned32 esr;

  Unsigned64 mair;
  Unsigned64 amair;

  Unsigned64 vbar;
  Unsigned64 tpidr_el1;
  Unsigned64 sp_el1;
  Unsigned64 elr_el1;

  Unsigned64 far;
  Unsigned64 par;

  Unsigned32 spsr_el1;
  Unsigned32 spsr_abt;
  Unsigned32 spsr_fiq;
  Unsigned32 spsr_irq;
  Unsigned32 spsr_und;

  Unsigned32 afsr[2];
  Unsigned32 mdcr;
  Unsigned32 mdscr;
  Unsigned32 contextidr;

  Unsigned32 dacr32;
  Unsigned32 fpexc32;
  Unsigned32 ifsr32;
};
