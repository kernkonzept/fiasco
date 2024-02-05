IMPLEMENTATION [arm && cpu_virt]:

IMPLEMENT_OVERRIDE inline
Mword
Thread::arch_check_vcpu_state(bool ext)
{
  if (ext && !check_for_current_cpu())
    return -L4_err::EInval;

  return 0;
}

IMPLEMENT inline
Mword
Thread::user_flags() const
{
  return ((_hyp.hcr & Cpu::Hcr_tge) ? Exr_arm_set_el_el0
                                    : Exr_arm_set_el_el1);
}

IMPLEMENT_OVERRIDE
bool
Thread::ex_regs_arch(Mword ops)
{
  if (ops & Exr_arm_unassigned)
    return false;

  bool tge;

  switch (ops & Exr_arm_set_el_mask)
    {
    case Exr_arm_set_el_keep:
      return true;
    case Exr_arm_set_el_el0:
      tge = true;
      break;
    case Exr_arm_set_el_el1:
      tge = false;
      break;
    default:
      return false;
    }

  // No change of exception level allowed after extended vCPU was enabled. The
  // state is then controlled by the corresponding
  // Hyp_vm_state.{host_regs,guest_regs} fields instead!
  if (state() & Thread_ext_vcpu_enabled)
    return false;

  if (tge)
    {
      regs()->psr &= ~Proc::Status_mode_mask;
      regs()->psr |= Proc::Status_mode_user_el0;
      _hyp.hcr |= Cpu::Hcr_tge;
    }
  else
    {
      regs()->psr &= ~Proc::Status_mode_mask;
      regs()->psr |= Proc::Status_mode_user_el1;
      _hyp.hcr &= ~Cpu::Hcr_tge;
    }

  if (current() == this)
    {
      assert(!(state() & Thread_ext_vcpu_enabled)); // see the test above
      _hyp.load(/*from_privileged*/ false, /*to_privileged*/ !tge);
    }

  return true;
}


IMPLEMENTATION [arm && 32bit && cpu_virt]:

#include "slowtrap_entry.h"
#include "infinite_loop.h"

IMPLEMENT_OVERRIDE
void
Thread::arch_init_vcpu_state(Vcpu_state *vcpu_state, bool ext)
{
  vcpu_state->version = Vcpu_arch_version;

  if (!ext || (state() & Thread_ext_vcpu_enabled))
    return;

  Vm_state::Vm_info *info = offset_cast<Vm_state::Vm_info *>(vcpu_state, 0x200);

  info->setup();

  Vm_state *v = vm_state(vcpu_state);

  v->csselr = 0;
  v->sctlr = (Cpu::sctlr | Cpu::Cp15_c1_cache_bits) & ~(Cpu::Cp15_c1_mmu | (1 << 28));
  v->actlr = 0;
  v->cpacr = 0x5755555;
  v->fcseidr = 0;
  v->vbar = 0;
  v->amair0 = 0;
  v->amair1 = 0;
  v->cntvoff = 0;

  v->guest_regs.hcr = Cpu::Hcr_tge | Cpu::Hcr_must_set_bits | Cpu::Hcr_dc;
  v->guest_regs.sctlr = 0;

  v->host_regs.hcr = Cpu::Hcr_host_bits | (_hyp.hcr & Cpu::Hcr_tge);

  Gic_h_global::gic->setup_state(&v->gic);

  if (current() == this)
    {
      asm volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r"(v->sctlr));
      asm volatile ("mcr p15, 4, %0, c1, c1, 3" : : "r"(Cpu::Hstr_vm)); // HSTR
      if (_hyp.hcr & Cpu::Hcr_tge)
        {
          // _hyp.hcr actually loaded in Context::arm_ext_vcpu_load_guest_regs()
          // but load it here as well to keep the code simple.
          _hyp.load(/*from_privileged*/ false, /*to_privileged*/ true);
        }
    }

  // use the real MPIDR as initial value, we might change this later
  // on and mask bits that should not be known to the user
  asm ("mrc p15, 0, %0, c0, c0, 5" : "=r" (v->vmpidr));

  // use the real MIDR as initial value
  asm ("mrc p15, 0, %0, c0, c0, 0" : "=r" (v->vpidr));
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && !debug]:

#include "infinite_loop.h"

extern "C" void hyp_mode_fault(Mword abort_type, Trap_state *ts)
{
  Mword v;
  Mword hsr;
  asm volatile("mrc p15, 4, %0, c5, c2, 0" : "=r" (hsr));

  switch (abort_type)
    {
    case 0:
    case 1:
      ts->esr.ec() = abort_type ? 0x11 : 0;
      printf("KERNEL%d: %s fault at lr=%lx pc=%lx hsr=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             abort_type ? "SWI" : "Undefined instruction",
             ts->km_lr, ts->pc, hsr);
      break;
    case 2:
      ts->esr.ec() = 0x21;
      asm volatile("mrc p15, 4, %0, c6, c0, 2" : "=r"(v));
      printf("KERNEL%d: Instruction abort at %lx hsr=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             v, hsr);
      break;
    case 3:
      ts->esr.ec() = 0x25;
      asm volatile("mrc p15, 4, %0, c6, c0, 0" : "=r"(v));
      printf("KERNEL%d: Data abort: pc=%lx pfa=%lx hsr=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             ts->ip(), v, hsr);
      break;
    default:
      printf("KERNEL%d: Unknown hyp fault at %lx hsr=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()), ts->ip(), hsr);
      break;
    };

  ts->dump();

  printf("In-kernel fault -- halting!\n");
  L4::infinite_loop();
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && debug]:

#include "infinite_loop.h"

PUBLIC static inline NEEDS[Thread::call_nested_trap_handler]
void
Thread::handle_hyp_mode_fault(Trap_state *ts)
{
  call_nested_trap_handler(ts);
}

extern "C" void hyp_mode_fault(Mword abort_type, Trap_state *ts)
{
  Mword hsr;
  asm volatile("mrc p15, 4, %0, c5, c2, 0" : "=r" (hsr));

  switch (abort_type)
    {
    case 0:
    case 1:
    case 2:
    case 3:
      Thread::handle_hyp_mode_fault(ts);
      return;

    default:
      // Cannot happen -- see Assembler stub.
      printf("KERNEL%d: Unknown hyp fault at %lx hsr=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()), ts->ip(), hsr);
      break;
    };

  ts->dump();

  kdb_ke("In-kernel fault");

  printf("Return from debugger -- halting!\n");
  L4::infinite_loop();
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

  return false;
}

//-----------------------------------------------------------------------------
INTERFACE [arm && cpu_virt]:

class Hyp_irqs
{
public:
  static unsigned vgic();
  static unsigned vtimer();
};

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

  Trap_state *ts =
    static_cast<Trap_state *>(static_cast<Return_frame *>(regs()));

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

  void alloc(Cpu_number cpu)
  {
    check (Irq_mgr::mgr->alloc(this, _irq, false));
    chip()->set_mode_percpu(cpu, pin(), Irq_chip::Mode::F_level_high);
    chip()->unmask_percpu(cpu, pin());
  }

private:
  void switch_mode(bool) override {}

  unsigned _virq;
  unsigned _irq;
};

PUBLIC inline FIASCO_FLATTEN
void
Arm_ppi_virt::handle(Upstream_irq const *ui)
{
  current_thread()->vcpu_vgic_upcall(_virq);
  chip()->ack(pin());
  Upstream_irq::ack(ui);
}

class Arm_vtimer_ppi : public Irq_base
{
public:
  Arm_vtimer_ppi(unsigned irq) : _irq(irq)
  {
    set_hit(handler_wrapper<Arm_vtimer_ppi>);
  }

  void alloc(Cpu_number cpu)
  {
    check (Irq_mgr::mgr->alloc(this, _irq, false));
    chip()->set_mode_percpu(cpu, pin(), Irq_chip::Mode::F_level_high);
    chip()->unmask_percpu(cpu, pin());
  }

private:
  void switch_mode(bool) override {}
  unsigned _irq;
};

PUBLIC inline NEEDS[Arm_vtimer_ppi::mask] FIASCO_FLATTEN
void
Arm_vtimer_ppi::handle(Upstream_irq const *ui)
{
  mask();
  current_thread()->vcpu_vgic_upcall(1);
  chip()->ack(pin());
  Upstream_irq::ack(ui);
}

IMPLEMENT_DEFAULT inline
unsigned Hyp_irqs::vgic()
{ return 25; }

IMPLEMENT_DEFAULT inline
unsigned Hyp_irqs::vtimer()
{ return 27; }

static Arm_ppi_virt __vgic_irq(Hyp_irqs::vgic(), 0);  // virtual GIC
static Arm_vtimer_ppi __vtimer_irq(Hyp_irqs::vtimer()); // virtual timer

namespace {
struct Local_irq_init
{
  explicit Local_irq_init(Cpu_number cpu)
  {
    if (cpu >= Cpu::invalid())
      return;

    __vgic_irq.alloc(cpu);
    __vtimer_irq.alloc(cpu);
  }
};

DEFINE_PER_CPU_LATE static Per_cpu<Local_irq_init>
  local_irqs(Per_cpu_data::Cpu_num);
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
Arm_esr
Thread::get_esr()
{
  Arm_esr hsr;
  asm ("mrc p15, 4, %0, c5, c2, 0" : "=r" (hsr));
  return hsr;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && cpu_virt && mmu]:

PRIVATE static inline NEEDS[Thread::invalid_pfa]
Address
Thread::get_fault_pfa(Arm_esr hsr, bool insn_abt, bool ext_vcpu)
{
  if (invalid_pfa(hsr))
    return ~0UL;

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

  Unsigned64 par, tmp;
  asm ("mrrc p15, 0, %Q0, %R0, c7 \n" // save guest PAR
       "mcr p15, 0, %2, c7, c8, 0 \n" // write guest virtual address to ATS1CPR
       "isb                       \n"
       "mrrc p15, 0, %Q1, %R1, c7 \n" // read translation result from PAR
       "mcrr p15, 0, %Q0, %R0, c7 \n" // restore guest PAR
       : "=&r"(tmp), "=r"(par)
       : "r"(far));
  if (par & 1)
    return ~0UL;
  return (par & 0xfffff000UL) | (far & 0xfff);
}

PUBLIC static inline template<typename T>
T
Thread::peek_user(T const *adr, Context *c)
{
  Address pa;
  asm ("mcr p15, 0, %1, c7, c8, 6 \n"
       "mrc p15, 0, %0, c7, c4, 0 \n"
       : "=r" (pa) : "r"(adr) );
  if (EXPECT_TRUE(!(pa & 1)))
    return *reinterpret_cast<T const *>(
              cxx::mask_lsb(pa, 12)
              | cxx::get_lsb(reinterpret_cast<Address>(adr), 12));

  c->set_kernel_mem_op_hit();
  return T(~0);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && cpu_virt && !mmu]:

PRIVATE static inline
Address
Thread::get_fault_pfa(Arm_esr /*hsr*/, bool insn_abt, bool /*ext_vcpu*/)
{
  Unsigned32 far;
  if (insn_abt)
    asm ("mrc p15, 4, %0, c6, c0, 2" : "=r" (far)); // HIFAR
  else
    asm ("mrc p15, 4, %0, c6, c0, 0" : "=r" (far)); // HDFAR

  return far;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && cpu_virt && arm_v8plus]:

PRIVATE static inline
bool
Thread::invalid_pfa(Arm_esr hsr)
{
  // FSC == 0b010001 is only documented for data aborts so this code works also
  // for instruction aborts.
  return hsr.pf_fsc() == 0x11 || hsr.pf_fnv();
}

// ---------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && cpu_virt && !arm_v8plus]:

PRIVATE static inline
bool
Thread::invalid_pfa(Arm_esr)
{
  return false;
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

  Vm_state::Vm_info *info = offset_cast<Vm_state::Vm_info *>(vcpu_state, 0x200);

  info->setup();

  Vm_state *v = vm_state(vcpu_state);

  v->sctlr = Cpu::Sctlr_el1_generic;
  v->actlr = 0;
  v->amair = 0;

  v->cntvoff = 0;
  v->guest_regs.hcr = Cpu::Hcr_tge | Cpu::Hcr_must_set_bits | Cpu::Hcr_dc;
  v->guest_regs.sctlr = 0;

  v->host_regs.hcr = Cpu::Hcr_host_bits | (_hyp.hcr & Cpu::Hcr_tge);

  Gic_h_global::gic->setup_state(&v->gic);

  v->vmpidr = _hyp.vmpidr;
  v->vpidr = _hyp.vpidr;

  if (current() == this)
    {
      asm volatile ("msr SCTLR_EL1, %x0" : : "r"(v->sctlr));
      if (_hyp.hcr & Cpu::Hcr_tge)
        {
          // Strictly speaking, the following registers would not need to be
          // loaded but do this here anyway to keep the code simple:
          // - _hyp.hcr is loaded in Context::arm_ext_vcpu_load_guest_regs()
          // - _hyp.cpacr is loaded in Context::arm_ext_vcpu_switch_to_guest()
          _hyp.load(/*from_privileged*/ false, /*to_privileged*/ true);
        }
    }
}
