INTERFACE:

EXTENSION class Hyp_vm_state
{
public:
  struct Regs_g
  {
    Unsigned64 hcr;

    Unsigned64 ttbr0;
    Unsigned64 ttbr1;
    Unsigned32 ttbcr;
    Unsigned32 sctlr;
    Unsigned32 dacr;
    Unsigned32 fcseidr;
    Unsigned32 cntv_ctl;
    Unsigned32 _res;
  };

  struct Regs_h
  {
    Unsigned64 hcr;
  };

  typedef Gic_h::Arm_vgic Gic;

  /* The following part is our user API */
  Regs_h host_regs;
  Regs_g guest_regs;

  Unsigned64 cntvoff;

  Unsigned32 vmpidr;
  Unsigned32 vpidr;

  Unsigned32 _res0[2];

  // size depdens on gic version, numer of LRs and APRs
  Gic  gic;

  /* The user API ends here */

  /* we should align this at a cache line ... */
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

  Unsigned32 mair0;
  Unsigned32 mair1;

  Unsigned32 amair0;
  Unsigned32 amair1;

  Unsigned32 vbar;

  Unsigned32 fcseidr;

  Unsigned32 fpinst;
  Unsigned32 fpinst2;
};

EXTENSION struct Context_hyp
{
public:
  // Banked registers for irq, svc, abt, and und modes
  struct Banked_mode_regs
  {
    Mword sp, lr, spsr;
  };

  // Banked registers for fiq mode
  struct Banked_fiq_regs
  {
    Mword r8, r9, r10, r11, r12, sp, lr, spsr;
  };

  // we need to store all banked registers for PL1 modes
  // because a hyp kernel runs applications in system mode (PL1)
  Banked_fiq_regs fiq;
  Banked_mode_regs irq, svc, abt, und;
};

//------------------------------------------------------------------
IMPLEMENTATION:

#include "cpu.h"

PUBLIC inline NEEDS["cpu.h"]
void
Context_hyp::save()
{
  asm volatile ("mrrc p15, 0, %Q0, %R0, c7" : "=r"(par));
  hcr = Cpu::hcr();
  // we do not save the CNTVOFF_EL2 because this kept in sync by the
  // VMM->VM switch code
  asm volatile ("mrrc p15, 3, %Q0, %R0, c14" : "=r" (cntv_cval));
  asm volatile ("mrc p15, 0, %0, c14, c1, 0" : "=r" (cntkctl));
  asm volatile ("mrc p15, 0, %0, c14, c3, 1" : "=r" (cntv_ctl));

  asm volatile ("mrc p15, 0, %0, c13, c0, 4" : "=r"(tpidrprw));
  asm volatile ("mrc p15, 0, %0, c13, c0, 1" : "=r"(contextidr));

#define STORER(m, r) asm volatile ("mrs %0, "#r"_"#m : "=r"(m.r))
#define STOREX(m) do { STORER(m, sp); STORER(m, lr); STORER(m, spsr); } while(0)
  STORER(fiq, r8);
  STORER(fiq, r9);
  STORER(fiq, r10);
  STORER(fiq, r11);
  STORER(fiq, r12);
  STOREX(fiq);
  STOREX(irq);
  STOREX(svc);
  STOREX(abt);
  STOREX(und);
#undef STOREX
#undef STORER
}

PUBLIC inline NEEDS["cpu.h"]
void
Context_hyp::load()
{
  asm volatile ("mcrr p15, 0, %Q0, %R0, c7" : : "r"(par));
  Cpu::hcr(hcr);
  asm volatile ("mcrr p15, 4, %Q0, %R0, c14" : : "r" (cntvoff));
  asm volatile ("mcrr p15, 3, %Q0, %R0, c14" : : "r" (cntv_cval));
  asm volatile ("mcr p15, 0, %0, c14, c1, 0" : : "r" (cntkctl));
  asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r" (cntv_ctl));

  asm volatile ("mcr p15, 0, %0, c13, c0, 4" : : "r"(tpidrprw));
  asm volatile ("mcr p15, 0, %0, c13, c0, 1" : : "r"(contextidr));

#define LOADR(m , r) asm volatile ("msr "#r"_"#m ", %0" : : "r"(m.r))
#define LOADX(m) do { LOADR(m, sp); LOADR(m, lr); LOADR(m, spsr); } while(0)
  LOADR(fiq, r8);
  LOADR(fiq, r9);
  LOADR(fiq, r10);
  LOADR(fiq, r11);
  LOADR(fiq, r12);
  LOADX(fiq);
  LOADX(irq);
  LOADX(svc);
  LOADX(abt);
  LOADX(und);
#undef LOADX
#undef LOADR
}
