INTERFACE:

EXTENSION class Hyp_vm_state
{
public:
  struct Regs_g
  {
    Unsigned64 hcr;

    Unsigned32 sctlr;
    Unsigned32 cpacr;
    Unsigned32 cntv_ctl;
    Unsigned32 _res;
  };

  struct Regs_h
  {
    Unsigned64 hcr;
  };

  typedef Gic_h_global::Arm_vgic Gic;

  /* The following part is our user API */
  Regs_h host_regs;
  Regs_g guest_regs;

  Unsigned64 cntvoff;

  Unsigned64 vmpidr;
  Unsigned32 vpidr;

  Unsigned32 _rsvd[3];

  // size depdens on gic version, numer of LRs and APRs
  Gic  gic;

  /* The user API ends here */

  /* we should align this at a cache line ... */
  Unsigned64 actlr;

  Unsigned64 tcr;
  Unsigned64 ttbr0;
  Unsigned64 ttbr1;

  Unsigned32 sctlr;
  Unsigned32 esr;

  Unsigned64 mair;
  Unsigned64 amair;

  Unsigned64 sp_el1;
  Unsigned64 elr_el1;

  Unsigned64 far;

  Unsigned32 afsr[2];

  Unsigned32 dacr32;
  Unsigned32 fpexc32;
  Unsigned32 ifsr32;
};

EXTENSION struct Context_hyp
{
public:
  Unsigned64 sp_el1;
  Unsigned64 elr_el1;
  Unsigned64 vbar;
  Unsigned32 cpacr = 0x0300000;
  // we need to store all banked registers for PL1 modes
  // because a hyp kernel runs applications in system mode (PL1)
  Unsigned32 spsr_fiq, spsr_irq, spsr_svc, spsr_abt, spsr_und;
  Unsigned32 csselr;

  // VM / USER RO but VMM writable
  Unsigned64 vmpidr = 1UL << 31;
  Unsigned32 vpidr = Cpu::midr();
};

//------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC inline
void
Context_hyp::save()
{
  asm volatile ("mrs %x0, PAR_EL1" : "=r"(par));
  asm volatile ("mrs %x0, HCR_EL2" : "=r"(hcr));

  // we do not save the CNTVOFF_EL2 because this kept in sync by the
  // VMM->VM switch code
  asm volatile ("mrs %x0, CNTV_CVAL_EL0"  : "=r"(cntv_cval));
  asm volatile ("mrs %x0, CNTKCTL_EL1"    : "=r"(cntkctl));
  asm volatile ("mrs %x0, CNTV_CTL_EL0"   : "=r"(cntv_ctl));
  asm volatile ("mrs %x0, TPIDR_EL1"      : "=r"(tpidrprw));
  asm volatile ("mrs %x0, CONTEXTIDR_EL1" : "=r"(contextidr));


  asm volatile ("mrs %x0, SP_EL1"    : "=r"(sp_el1));
  asm volatile ("mrs %x0, ELR_EL1"   : "=r"(elr_el1));
  asm volatile ("mrs %x0, VBAR_EL1"  : "=r"(vbar));
  asm volatile ("mrs %x0, CPACR_EL1" : "=r"(cpacr));

  asm volatile ("mrs %x0, SPSR_fiq"  : "=r"(spsr_fiq));
  asm volatile ("mrs %x0, SPSR_irq"  : "=r"(spsr_irq));
  asm volatile ("mrs %x0, SPSR_EL1"  : "=r"(spsr_svc));
  asm volatile ("mrs %x0, SPSR_abt"  : "=r"(spsr_abt));
  asm volatile ("mrs %x0, SPSR_und"  : "=r"(spsr_und));
  asm volatile ("mrs %x0, CSSELR_EL1": "=r"(csselr));
}

PUBLIC inline
void
Context_hyp::load()
{
  asm volatile ("msr PAR_EL1, %x0"        : : "r"(par));
  asm volatile ("msr HCR_EL2, %x0"        : : "r"(hcr));

  asm volatile ("msr CNTVOFF_EL2, %x0"    : : "r"(cntvoff));
  asm volatile ("msr CNTV_CVAL_EL0, %x0"  : : "r"(cntv_cval));
  asm volatile ("msr CNTKCTL_EL1, %x0"    : : "r"(cntkctl));
  asm volatile ("msr CNTV_CTL_EL0, %x0"   : : "r"(cntv_ctl));
  asm volatile ("msr TPIDR_EL1, %x0"      : : "r"(tpidrprw));
  asm volatile ("msr CONTEXTIDR_EL1, %x0" : : "r"(contextidr));

  asm volatile ("msr SP_EL1, %x0"         : : "r"(sp_el1));
  asm volatile ("msr ELR_EL1, %x0"        : : "r"(elr_el1));
  asm volatile ("msr VBAR_EL1, %x0"       : : "r"(vbar));
  asm volatile ("msr CPACR_EL1, %x0"      : : "r"(cpacr));

  asm volatile ("msr SPSR_fiq, %x0"       : : "r"(spsr_fiq));
  asm volatile ("msr SPSR_irq, %x0"       : : "r"(spsr_irq));
  asm volatile ("msr SPSR_EL1, %x0"       : : "r"(spsr_svc));
  asm volatile ("msr SPSR_abt, %x0"       : : "r"(spsr_abt));
  asm volatile ("msr SPSR_und, %x0"       : : "r"(spsr_und));
  asm volatile ("msr CSSELR_EL1, %x0"     : : "r"(csselr));
}

