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

//-----------------------------------------------------------------------------
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

  Vm_state::Vm_info *info
    = offset_cast<Vm_state::Vm_info *>(vcpu_state,
                                       Config::Ext_vcpu_infos_offset);

  info->setup();

  Vm_state *v = vm_state(vcpu_state);

  v->csselr = 0;
  v->sctlr = arm_host_sctlr();
  v->actlr = 0;
  v->cpacr = 0x5f55555;
  v->fcseidr = 0;
  v->vbar = 0;
  v->amair0 = 0;
  v->amair1 = 0;
  v->cntvoff = 0;

  v->guest_regs.hcr = Cpu::Hcr_tge | Cpu::Hcr_must_set_bits | Cpu::Hcr_dc;
  v->guest_regs.sctlr = 0;

  v->host_regs.hcr = Cpu::Hcr_host_bits | (_hyp.hcr & Cpu::Hcr_tge);

  Gic_h_global::gic->setup_state(&v->gic);

  // use the real MPIDR as initial value, we might change this later
  // on and mask bits that should not be known to the user
  asm ("mrc p15, 0, %0, c0, c0, 5" : "=r" (v->vmpidr));

  // use the real MIDR as initial value
  asm ("mrc p15, 0, %0, c0, c0, 0" : "=r" (v->vpidr));

  if (current() == this)
    {
      // See Hyp_vm_state.hcr why this test is safe.
      // Furthermore, if this test fails, the thread was already switched to
      // execute at PL1 and _hyp.load() was done in Thread::ex_regs_arch().
      if (_hyp.hcr & Cpu::Hcr_tge)
        {
          // _hyp.hcr actually loaded in Context::arm_ext_vcpu_load_guest_regs()
          // but load it here as well to keep the code simple.
          _hyp.load(/*from_privileged*/ false, /*to_privileged*/ true);
        }

      load_ext_vcpu_state(v);
    }
}

extern "C" void hyp_mode_fault(Mword abort_type, Trap_state *ts)
{
  Mword hsr;
  asm volatile("mrc p15, 4, %0, c5, c2, 0" : "=r" (hsr));

  switch (abort_type)
    {
    case 0:
    case 1:
      break;
    case 2:
      asm volatile("mrc p15, 4, %0, c6, c0, 2" : "=r"(ts->pf_address)); // HIFAR
      break;
    case 3:
      asm volatile("mrc p15, 4, %0, c6, c0, 0" : "=r"(ts->pf_address)); // HDFAR
      break;

    default:
      // Cannot happen -- see Assembler stub.
      printf("KERNEL%d: Unknown hyp fault %lu at %lx hsr=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()), abort_type, ts->ip(),
             hsr);
      break;
    };

  ts->error_code = hsr;
  if (Thread::handle_hyp_mode_fault(abort_type, ts, hsr))
    return;

  ts->dump();

  kdb_ke("In-kernel fault");

  printf("Unhandled in-kernel fault -- halting!\n");
  L4::infinite_loop();
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && cpu_virt && debug]:

PUBLIC static inline NEEDS[Thread::call_nested_trap_handler,
                           Thread::is_transient_mpu_fault]
bool
Thread::handle_hyp_mode_fault(Mword abort_type, Trap_state *ts, Mword hsr)
{
  if (is_transient_mpu_fault(abort_type, hsr))
    return true;

  if (call_nested_trap_handler(ts) == 0)
    return true; // Trap handled, try to continue.

  return false;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && cpu_virt && !debug]:

PUBLIC static inline NEEDS[Thread::is_transient_mpu_fault]
bool
Thread::handle_hyp_mode_fault(Mword abort_type, Trap_state *ts, Mword hsr)
{
  if (is_transient_mpu_fault(abort_type, hsr))
    return true;

  switch (abort_type)
    {
    case 0:
    case 1:
      printf("KERNEL%d: %s fault at lr=%lx pc=%lx hsr=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             abort_type ? "SWI" : "Undefined instruction",
             ts->km_lr, ts->pc, hsr);
      break;
    case 2:
    case 3:
      printf("KERNEL%d: %s abort: pc=%lx pfa=%lx hsr=%lx\n",
             cxx::int_value<Cpu_number>(current_cpu()),
             abort_type == 2 ? "Instruction" : "Data",
             ts->ip(), ts->pf_address, hsr);
      break;
    };

  return false;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && cpu_virt && mpu]:

#include "kmem.h"

EXTENSION class Thread
{
  static Global_data<bool> transient_fault_warned;
};

DEFINE_GLOBAL Global_data<bool> Thread::transient_fault_warned;

/**
 * Check for transient MPU fault caused by optimized entry code.
 *
 * The entry code manipulates the kernel heap-, kip- and ku_mem-MPU-regions.
 * Legally, an ISB instruction would be required but this is practically not
 * needed and would impose an undue performance penalty. Instead, handle such
 * data aborts gracefully in case they ever happen.
 */
PRIVATE static inline NEEDS["kmem.h"]
bool
Thread::is_transient_mpu_fault(Mword abort_type, Mword raw_hsr)
{
  Arm_esr hsr(raw_hsr);

  if (abort_type != 3)  // in-kernel data abort?
    return false;
  if (hsr.ec() != 0x25) // data-abort from same exception-level?
    return false;

  Mword pfa;
  asm volatile("mrc p15, 4, %0, c6, c0, 0" : "=r"(pfa));  // HDFAR

  switch (hsr.pf_fsc())
    {
    case 0b000100:  // level 0 translation fault
      // Only kernel heap region start/end is adapted on entry
      if (!(*Kmem::kdir)[Kpdir::Kernel_heap].contains(pfa))
        return false;
      break;
    case 0b001100:  // level 0 permission fault
      // Only the KIP permissions are manipulated.
      if (!(*Kmem::kdir)[Kpdir::Kip].contains(pfa))
        return false;
      break;
    default:
      return false;
    }

  Mem::isb();

  if (!transient_fault_warned)
    {
      transient_fault_warned = true;
      WARN("Unexpected in-kernel data abort. Add an ISB to entry path?\n");
    }

  return true;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && cpu_virt && !mpu]:

PRIVATE static inline
bool
Thread::is_transient_mpu_fault(Mword, Mword)
{ return false; }

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && cpu_virt && fpu]:

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
IMPLEMENTATION [arm && 32bit && cpu_virt]:

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

  Vm_state::Vm_info *info
    = offset_cast<Vm_state::Vm_info *>(vcpu_state,
                                       Config::Ext_vcpu_infos_offset);

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
      asm volatile ("msr HSTR_EL2, %x0" : : "r"(Cpu::Hstr_vm));
      // See Hyp_vm_state.hcr why this test is safe.
      // Furthermore, if this test fails, the thread was already switched to
      // execute at EL1 and _hyp.load() was done in Thread::ex_regs_arch().
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
