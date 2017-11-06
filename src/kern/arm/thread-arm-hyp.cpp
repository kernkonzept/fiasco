IMPLEMENTATION [arm && 32bit && cpu_virt]:

IMPLEMENT_OVERRIDE
void
Thread::arch_init_vcpu_state(Vcpu_state *vcpu_state, bool ext)
{
  vcpu_state->version = Vcpu_arch_version;

  if (!ext || (state() & Thread_ext_vcpu_enabled))
    return;

  assert (check_for_current_cpu());

  Vm_state *v = vm_state(vcpu_state);
  v->hcr = Cpu::Hcr_host_bits;
  v->csselr = 0;
  v->sctlr = (Cpu::sctlr | Cpu::Cp15_c1_cache_bits) & ~(Cpu::Cp15_c1_mmu | (1 << 28));
  v->actlr = 0;
  v->cpacr = 0x5555555;
  v->fcseidr = 0;
  v->contextidr = 0;
  v->cntkctl = Host_cntkctl; // allow PL0 access to CNTV
  v->cntvoff = 0;
  v->vbar = 0;
  v->amair0 = 0;
  v->amair1 = 0;

  v->guest_regs.hcr = Cpu::Hcr_tge | Cpu::Hcr_must_set_bits;
  v->guest_regs.sctlr = 0;

  v->host_regs.hcr = Cpu::Hcr_host_bits;
  v->svc.lr = regs()->ulr;
  v->svc.sp = regs()->sp();

  v->gic.hcr = Gic_h::Hcr(0);
  v->gic.apr = 0;

  if (current() == this)
    {
      asm volatile ("mcr p15, 4, %0, c1, c1, 0" : : "r"(Cpu::Hcr_host_bits));
      asm volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r"(v->sctlr));
      asm volatile ("mcr p15, 0, %0, c14, c1, 0" : : "r"(v->cntkctl));
      asm volatile ("mcrr p15, 4, %Q0, %R0, c14" : : "r"(v->cntvoff));
    }

  // use the real MPIDR as initial value, we might change this later
  // on and mask bits that should not be known to the user
  asm ("mrc p15, 0, %0, c0, c0, 5" : "=r" (v->vmpidr));

}

extern "C" void slowtrap_entry(Trap_state *ts);
extern "C" Mword pagefault_entry(const Mword pfa, Mword error_code,
                                 const Mword pc, Return_frame *ret_frame);

PUBLIC static inline template<typename T>
T
Thread::peek_user(T const *adr, Context *c)
{
  Address pa;
  asm ("mcr p15, 0, %1, c7, c8, 6 \n"
       "mrc p15, 0, %0, c7, c4, 0 \n"
       : "=r" (pa) : "r"(adr) );
  if (EXPECT_TRUE(!(pa & 1)))
    return *reinterpret_cast<T const *>(cxx::mask_lsb(pa, 12)
                                        | cxx::get_lsb((Address)adr, 12));

  c->set_kernel_mem_op_hit();
  return T(~0);
}

PRIVATE static inline
Mword
Thread::get_lr_for_mode(Return_frame const *rf)
{
  Mword ret;
  switch (rf->psr & 0x1f)
    {
    case Proc::PSR_m_usr:
    case Proc::PSR_m_sys:
      return rf->ulr;
    case Proc::PSR_m_irq:
      asm ("mrs %0, lr_irq" : "=r" (ret)); return ret;
    case Proc::PSR_m_fiq:
      asm ("mrs %0, lr_fiq" : "=r" (ret)); return ret;
    case Proc::PSR_m_abt:
      asm ("mrs %0, lr_abt" : "=r" (ret)); return ret;
    case Proc::PSR_m_svc:
      asm ("mrs %0, lr_svc" : "=r" (ret)); return ret;
    case Proc::PSR_m_und:
      asm ("mrs %0, lr_und" : "=r" (ret)); return ret;
    default:
      assert(false); // wrong processor mode
      return ~0UL;
    }
}

extern "C" void hyp_mode_fault(Mword abort_type, Trap_state *ts)
{
  Mword v;
  switch (abort_type)
    {
    case 0:
    case 1:
      ts->esr.ec() = abort_type ? 0x11 : 0;
      printf("KERNEL%d: %s fault at %lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             abort_type ? "SWI" : "Undefined instruction",
             ts->km_lr);
      break;
    case 2:
      ts->esr.ec() = 0x21;
      asm volatile("mrc p15, 4, %0, c6, c0, 2" : "=r"(v));
      printf("KERNEL%d: Instruction abort at %lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             v);
      break;
    case 3:
      ts->esr.ec() = 0x25;
      asm volatile("mrc p15, 4, %0, c6, c0, 0" : "=r"(v));
      printf("KERNEL%d: Data abort: pc=%lx pfa=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             ts->ip(), v);
      break;
    default:
      printf("KERNEL%d: Unknown hyp fault at %lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             ts->ip());
      break;
    };

  ts->dump();

  kdb_ke("In-kernel fault");
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && fpu && 32bit]:

PUBLIC static
bool
Thread::handle_fpu_trap(Trap_state *ts)
{
  unsigned cond = ts->esr.cv() ? ts->esr.cond() : 0xe;
  if (!Thread::condition_valid(cond, ts->psr))
    {
      // FPU insns are 32bit, even for thumb
      assert (ts->esr.il());
      ts->pc += 4;
      return true;
    }

  assert (!Fpu::is_enabled());

  if (current_thread()->switchin_fpu())
    return true;

  // emulate the ARM exception entry PC
  ts->pc += ts->psr & Proc::Status_thumb ? 2 : 4;

  return false;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

#include "irq_mgr.h"

PUBLIC inline
void
Thread::vcpu_vgic_upcall(unsigned virq)
{
  assert (state() & Thread_ext_vcpu_enabled);
  assert (state() & Thread_vcpu_user);
  assert (!_exc_cont.valid(regs()));

  Vcpu_state *vcpu = vcpu_state().access();
  assert (vcpu_exceptions_enabled(vcpu));

  Trap_state *ts = static_cast<Trap_state *>((Return_frame *)regs());

  // Before entering kernel mode to have original fpu state before
  // enabling FPU
  save_fpu_state_to_utcb(ts, utcb().access());
  spill_user_state();

  check (vcpu_enter_kernel_mode(vcpu));
  vcpu = vcpu_state().access();

  vcpu->_regs.s.esr.ec() = 0x3d;
  vcpu->_regs.s.esr.svc_imm() = virq;

  vcpu_save_state_and_upcall();
}


class Arm_ppi_virt : public Irq_base
{
public:

  Arm_ppi_virt(unsigned irq, unsigned virq) : _virq(virq), _irq(irq)
  {
    set_hit(handler_wrapper<Arm_ppi_virt>);
  }

  void alloc()
  {
    printf("Allocate ARM PPI %d to virtual %d\n", _irq, _virq);
    check (Irq_mgr::mgr->alloc(this, _irq));
    chip()->unmask(pin());
  }

private:
  void switch_mode(bool) {}

  unsigned _virq;
  unsigned _irq;
};

PUBLIC inline FIASCO_FLATTEN
void
Arm_ppi_virt::handle(Upstream_irq const *ui)
{
  current_thread()->vcpu_vgic_upcall(_virq);
  chip()->ack(pin());
  ui->ack();
}

class Arm_vtimer_ppi : public Irq_base
{
public:
  Arm_vtimer_ppi(unsigned irq) : _irq(irq)
  {
    set_hit(handler_wrapper<Arm_vtimer_ppi>);
  }

  void alloc()
  {
    printf("Allocate ARM PPI %d to virtual %d\n", _irq, 1);
    check (Irq_mgr::mgr->alloc(this, _irq));
    chip()->unmask(pin());
  }

private:
  void switch_mode(bool) {}
  unsigned _irq;
};

PUBLIC inline NEEDS[Arm_vtimer_ppi::mask] FIASCO_FLATTEN
void
Arm_vtimer_ppi::handle(Upstream_irq const *ui)
{
  mask();
  current_thread()->vcpu_vgic_upcall(1);
  chip()->ack(pin());
  ui->ack();
}

static Arm_ppi_virt __vgic_irq(25, 0);  // virtual GIC
static Arm_vtimer_ppi __vtimer_irq(27); // virtual timer

namespace {
struct Local_irq_init
{
  Local_irq_init()
  {
    __vgic_irq.alloc();
    __vtimer_irq.alloc();
  }
};
DEFINE_PER_CPU_LATE static Per_cpu<Local_irq_init> local_irqs;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && cpu_virt]:

PRIVATE inline
void
Arm_vtimer_ppi::mask()
{
  Mword v;
  asm volatile("mrc p15, 0, %0, c14, c3, 1\n"
               "orr %0, #0x2              \n"
               "mcr p15, 0, %0, c14, c3, 1\n" : "=r" (v));
}

PRIVATE static inline
bool
Thread::is_syscall_pc(Address pc)
{
  return Address(-0x0c) <= pc && pc <= Address(-0x08);
}

PRIVATE static inline
Address
Thread::get_fault_pfa(Arm_esr hsr, bool insn_abt, bool ext_vcpu)
{
  Unsigned32 far;
  if (insn_abt)
    asm ("mrc p15, 4, %0, c6, c0, 2" : "=r" (far));
  else
    asm ("mrc p15, 4, %0, c6, c0, 0" : "=r" (far));

  if (EXPECT_TRUE(!ext_vcpu))
    return far;

  Unsigned32 sctlr;
  asm ("mrc p15, 0, %0, c1, c0, 0" : "=r" (sctlr));
  if (!(sctlr & 1)) // stage 1 mmu disabled
    return far;

  if (hsr.pf_s1ptw()) // stage 1 walk
    {
      Unsigned32 ipa;
      asm ("mrc p15, 4, %0, c6, c0, 4" : "=r" (ipa));
      return ipa << 8;
    }

  if ((hsr.pf_fsc() & 0x3c) != 0xc) // no permission fault
    {
      Unsigned32 ipa;
      asm ("mrc p15, 4, %0, c6, c0, 4" : "=r" (ipa));
      return (ipa << 8) | (far & 0xfff);
    }

  Unsigned64 par;
  asm ("mcr p15, 0, %1, c7, c8, 0 \n"
       "mrrc p15, 0, %Q0, %R0, c7 \n" : "=r"(par) : "r"(far));
  if (par & 1)
    return ~0UL;
  return (par & 0xfffff000UL) | (far & 0xfff);
}

PRIVATE static inline
Arm_esr
Thread::get_esr()
{
  Arm_esr hsr;
  asm ("mrc p15, 4, %0, c5, c2, 0" : "=r" (hsr));
  return hsr;
}

// ---------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && cpu_virt]:

IMPLEMENT_OVERRIDE
void
Thread::arch_init_vcpu_state(Vcpu_state *vcpu_state, bool ext)
{
  vcpu_state->version = Vcpu_arch_version;

  if (!ext || (state() & Thread_ext_vcpu_enabled))
    return;

  assert (check_for_current_cpu());

  Vm_state *v = vm_state(vcpu_state);
  v->hcr = 0;
  v->csselr = 0;
  v->sctlr = Cpu::Sctlr_el1_generic;
  v->actlr = 0;
  v->cpacr = 0x5555555;
  v->vbar = 0;
  v->amair = 0;

  v->guest_regs.hcr = Cpu::Hcr_tge;
  v->guest_regs.sctlr = 0;
  v->guest_regs.mdscr = 0;

  v->host_regs.hcr = arm_get_hcr();
  v->host_regs.mdscr = 0;
  v->cntkctl = Host_cntkctl;
  v->cntvoff = 0;

  v->gic.hcr = Gic_h::Hcr(0);
  v->gic.apr = 0;
  v->vmpidr = 1UL << 31; // ARMv8: RES1

  if (current() == this)
    {
      asm volatile ("msr SCTLR_EL1, %0" : : "r"(v->sctlr));
      asm volatile ("msr CNTKCTL_EL1, %0" : : "r"(v->cntkctl));
      asm volatile ("msr CNTVOFF_EL2, %0" : : "r"(v->cntvoff));
    }

  //regs()->pstate = (regs()->pstate & ~0x1fUL) | Proc::Status_mode_vmm;
}
