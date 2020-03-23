INTERFACE:

EXTENSION class Hyp_vm_state
{
public:
  struct Per_mode_regs
  {
    Mword sp;
    Mword lr;
    Mword spsr;
  };

  struct Regs
  {
    Unsigned32 hcr;

    Unsigned64 ttbr0;
    Unsigned64 ttbr1;
    Unsigned32 ttbcr;
    Unsigned32 sctlr;
    Unsigned32 dacr;
    Unsigned32 fcseidr;
    Unsigned32 contextidr;
    Unsigned32 cntkctl;
  };

  typedef Gic_h::Arm_vgic Gic;

  /* The following part is our user API */
  Regs guest_regs;
  Regs host_regs;

  Unsigned64 cntvoff;

  Unsigned64 cntv_cval;
  Unsigned32 cntkctl;
  Unsigned32 cntv_ctl;

  Unsigned32 vmpidr;
  Unsigned32 vpidr;

  // size depdens on gic version, numer of LRs and APRs
  Gic  gic;

  /* The user API ends here */

  /* we should align this at a cache line ... */
  Unsigned32 hcr;

  Unsigned32 csselr;

  Unsigned32 sctlr;
  Unsigned32 actlr;
  Unsigned32 cpacr;

  Unsigned64 ttbr0;
  Unsigned64 ttbr1;
  Unsigned32 ttbcr;

  Unsigned32 dacr;

  Unsigned32 dfsr;
  Unsigned32 ifsr;
  Unsigned32 adfsr;
  Unsigned32 aifsr;

  Unsigned32 dfar;
  Unsigned32 ifar;

  Unsigned64 par;

  Unsigned32 mair0;
  Unsigned32 mair1;

  Unsigned32 amair0;
  Unsigned32 amair1;

  Unsigned32 vbar;

  Unsigned32 fcseidr;
  Unsigned32 contextidr;
  Unsigned32 tpidrprw;

  Per_mode_regs svc;
  Per_mode_regs abt;
  Per_mode_regs und;
  Per_mode_regs irq;
  Mword fiq_r8;
  Mword fiq_r9;
  Mword fiq_r10;
  Mword fiq_r11;
  Mword fiq_r12;
  Per_mode_regs fiq;

  Unsigned32 fpinst;
  Unsigned32 fpinst2;
};
