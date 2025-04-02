INTERFACE [vmx]:

#include "config.h"
#include "per_cpu_data.h"
#include "vm.h"
#include "vmx.h"
#include "ipi.h"

class Vmcs;

class Vm_vmx_base : public Vm
{
protected:
  static unsigned long resume_vm_vmx(Trex *regs)
    asm("resume_vm_vmx") __attribute__((__regparm__(3)));

  enum
  {
    EFER_LME = 1 << 8,
    EFER_LMA = 1 << 10,
  };

  enum
  {
    Exit_irq           = 1U,
    Exit_ept_violation = 48U,
  };
};

template<typename VARIANT>
class Vm_vmx_t : public Vm_vmx_base
{
};

/**
 * VMX implementation variant without EPT support.
 */
class Vm_vmx : public Vm_vmx_t<Vm_vmx>
{
};

//----------------------------------------------------------------------------
IMPLEMENTATION [vmx]:

#include "context.h"
#include "cpu.h"
#include "fpu.h"
#include "mem_space.h"
#include "msrdefs.h"
#include "idt.h"
#include "thread.h" // XXX: circular dep, move this out here!
#include "thread_state.h" // XXX: circular dep, move this out here!
#include "virt.h"

static Kmem_slab_t<Vm_vmx> _vmx_allocator("Vm_vmx");

PUBLIC inline
Vm_vmx_base::Vm_vmx_base(Ram_quota *q) : Vm(q)
{}

PUBLIC inline template<typename VARIANT>
Vm_vmx_t<VARIANT>::Vm_vmx_t(Ram_quota *q) : Vm_vmx_base(q)
{}

PUBLIC
Vm_vmx::Vm_vmx(Ram_quota *q) : Vm_vmx_t<Vm_vmx>(q)
{
  _tlb_type = Tlb_per_cpu_global;
}

PUBLIC static
Vm_vmx *Vm_vmx::alloc(Ram_quota *q)
{
  return _vmx_allocator.q_new(q, q);
}

PUBLIC inline
void *
Vm_vmx::operator new (size_t size, void *p) noexcept
{
  (void)size;
  assert (size == sizeof (Vm_vmx));
  return p;
}

PUBLIC
void
Vm_vmx::operator delete (Vm_vmx *vm, std::destroying_delete_t)
{
  Ram_quota *q = vm->ram_quota();
  vm->~Vm_vmx();
  _vmx_allocator.q_free(q, vm);
}

PUBLIC
void
Vm_vmx::tlb_flush_current_cpu() override
{
  // Nothing to do here, we flush on each entry!
}

PUBLIC inline
void
Vm_vmx::load_vm_memory(Vmx_vm_state *vm_state)
{
  if (sizeof(long) > sizeof(int))
    {
      if (vm_state->read<Vmx::Vmcs_guest_ia32_efer>() & EFER_LME)
        Vmx::vmcs_write<Vmx::Vmcs_guest_cr3>(phys_dir());
      else
        WARN("VMX: No, not possible\n");
    }
  else
    {
      // for 32bit we can just load the Vm pdbr
      Vmx::vmcs_write<Vmx::Vmcs_guest_cr3>(phys_dir());
    }

  tlb_mark_used();
}

PUBLIC inline
void
Vm_vmx::store_vm_memory(Vmx_vm_state *)
{
  // we flush the tlb on each entry
  tlb_mark_unused();
}

PROTECTED inline
template<typename VARIANT>
template<Host_state HOST_STATE>
unsigned long
Vm_vmx_t<VARIANT>::vm_entry(Trex *regs,
                            Vmx_vm_state_t<HOST_STATE> *vm_state,
                            bool guest_long_mode, Unsigned32 *reason)
{
  Cpu &cpu = Cpu::cpus.current();
  Vmx &vmx = Vmx::cpus.current();

  vm_state->load_guest_state();
  static_cast<VARIANT *>(this)->load_vm_memory(vm_state);

  // Set volatile host state
  Vmx::vmcs_write<Vmx::Vmcs_host_cr0>(Cpu::get_cr0());
  Vmx::vmcs_write<Vmx::Vmcs_host_cr3>(Cpu::get_pdbr()); // host_area.cr3
  Vmx::vmcs_write<Vmx::Vmcs_host_cr4>(Cpu::get_cr4());

  Gdt *gdt = cpu.get_gdt();
  Unsigned16 tr = Cpu::get_tr();

  Vmx::vmcs_write<Vmx::Vmcs_host_tr_base>(((*gdt)[tr / 8]).base());
  Vmx::vmcs_write<Vmx::Vmcs_host_ia32_sysenter_esp>
                 (reinterpret_cast<Mword>(&cpu.kernel_sp()));

  Pseudo_descriptor pseudo;
  gdt->get(&pseudo);
  Vmx::vmcs_write<Vmx::Vmcs_host_gdtr_base>(pseudo.base());

  if (cpu.features() & FEAT_PAT
      && vmx.info.exit_ctls.allowed(Vmx_info::Ex_load_ia32_pat))
    Vmx::vmcs_write<Vmx::Vmcs_host_ia32_pat>(Cpu::rdmsr(Msr::Ia32_pat));

  if (vmx.info.exit_ctls.allowed(Vmx_info::Ex_load_ia32_efer))
    Vmx::vmcs_write<Vmx::Vmcs_host_ia32_efer>(Cpu::rdmsr(Msr::Ia32_efer));

  if (vmx.info.exit_ctls.allowed(Vmx_info::Ex_load_perf_global_ctl))
    Vmx::vmcs_write<Vmx::Vmcs_host_ia32_perf_global_ctrl>(
                                         Cpu::rdmsr(Msr::Ia32_perf_global_ctrl));

  safe_host_segments();

  if (EXPECT_FALSE(Vmx::vmx_failure()))
    return 1;

  Unsigned16 ldt = Cpu::get_ldt();

  // Set guest CR2
    {
      Mword vm_guest_cr2 = vm_state->template read<Vmx::Sw_guest_cr2>();

      asm volatile (
        "mov %[vm_guest_cr2], %%cr2"
        :
        : [vm_guest_cr2] "r" (vm_guest_cr2)
      );
    }

  load_guest_msrs(vm_state, guest_long_mode);

  Unsigned64 guest_xcr0 = vm_state->template read<Vmx::Sw_guest_xcr0>();
  Unsigned64 host_xcr0 = Fpu::fpu.current().get_xcr0();

  load_guest_xcr0(host_xcr0, guest_xcr0);

  if (cpu.has_l1d_flush() && !cpu.skip_l1dfl_vmentry())
    Cpu::wrmsr(1UL, Msr::Ia32_flush_cmd);

  unsigned long ret = resume_vm_vmx(regs);

  restore_host_xcr0(host_xcr0, guest_xcr0);

  Unsigned32 error = 0;
  if (EXPECT_FALSE(ret))
    {
      // We read the VM instruction error here to make sure that the original
      // value has not been accidentally overwritten by any following VMX
      // instruction (e.g. VMREAD).
      //
      // However, we defer the actual handling of the resume failure until the
      // host state has been properly restored.
      error = Vmx::vmcs_read<Vmx::Vmcs_vm_insn_error>();
    }

  store_guest_restore_host_msrs(vm_state, guest_long_mode);

  // Save guest CR2
    {
      Mword vm_guest_cr2;

      asm volatile(
        "mov %%cr2, %[vm_guest_cr2]"
        : [vm_guest_cr2] "=r" (vm_guest_cr2)
      );

      vm_state->template write<Vmx::Sw_guest_cr2>(vm_guest_cr2);
    }

  // GDT limit now at 0xffff+1: restore our limit
  // IDT limit now at 0xffff+1: we don't care
  // LDT value now at 0: restore our value
  cpu.set_gdt();
  Cpu::set_ldt(ldt);

  // reload TSS, we use I/O bitmaps
  // ... do this lazy ...
    {
      // clear busy flag
      Gdt_entry *e = &(*gdt)[Gdt::gdt_tss / 8];
      e->tss_make_available();
      asm volatile("" : : "m" (*e));
      Cpu::set_tr(Gdt::gdt_tss);
    }

  *reason = Vmx::vmcs_read<Vmx::Vmcs_exit_reason>();

  vm_state->store_guest_state();
  static_cast<VARIANT *>(this)->store_vm_memory(vm_state);
  vm_state->store_exit_info(error, *reason);

  return ret;
}

PROTECTED inline template<typename VARIANT>
int
Vm_vmx_t<VARIANT>::do_resume_vcpu(Context *ctxt, Vcpu_state *vcpu,
                                  Vmx_vm_state *vm_state)
{
  assert(cpu_lock.test());

  /* these 4 must not use ldt entries */
  assert(!(Cpu::get_cs() & (1 << 2)));
  assert(!(Cpu::get_ss() & (1 << 2)));
  assert(!(Cpu::get_ds() & (1 << 2)));
  assert(!(Cpu::get_es() & (1 << 2)));

  Cpu_number const cpu = current_cpu();
  Vmx &vmx = Vmx::cpus.cpu(cpu);

  if (!vmx.vmx_enabled())
    {
      WARNX(Info, "VMX: not supported/enabled\n");
      return -L4_err::ENodev;
    }

  int rv = vm_state->setup_vmcs(ctxt);
  if (rv != 0)
    return rv;

  rv = vmx.load_vmx_vmcs(ctxt->vmcs());
  if (rv != 0)
    return rv;

#ifdef CONFIG_LAZY_FPU
  // XXX:
  // This generates a circular dep between thread<->task, this cries for a
  // new abstraction...
  if (!(ctxt->state() & Thread_fpu_owner))
    {
      if (EXPECT_FALSE(!static_cast<Thread *>(ctxt)->switchin_fpu()))
        {
          WARN("VMX: switchin_fpu failed\n");
          return -L4_err::EInval;
        }
    }
#else
  (void)ctxt;
#endif

  bool guest_long_mode = vm_state->read<Vmx::Vmcs_vm_entry_ctls>() & (1U << 9);
  if (!is_64bit() && guest_long_mode)
    return -L4_err::EInval;

  Unsigned32 reason = 0;
  unsigned long ret = vm_entry(&vcpu->_regs, vm_state, guest_long_mode,
                               &reason);
  if (EXPECT_FALSE(ret))
    return -L4_err::EInval;

  Unsigned32 basic_reason = reason & 0xffffU;
  switch (basic_reason)
    {
    case Exit_irq:
      return 1;
    case Exit_ept_violation:
      vm_state->store_guest_physical_address();
      break;
    }

  force_kern_entry_vcpu_state(vcpu);
  return 0;
}

PUBLIC inline template<typename VARIANT>
int
Vm_vmx_t<VARIANT>::resume_vcpu(Context *ctxt, Vcpu_state *vcpu,
                               bool user_mode) override
{
  (void)user_mode;
  assert(user_mode);

  if (EXPECT_FALSE(!(ctxt->state(true) & Thread_ext_vcpu_enabled)))
    {
      ctxt->arch_load_vcpu_kern_state(vcpu, true);
      force_kern_entry_vcpu_state(vcpu);
      return -L4_err::EInval;
    }

  Vmx_vm_state *vm_state
    = offset_cast<Vmx_vm_state *>(vcpu, Config::Ext_vcpu_state_offset);

  for (;;)
    {
      // In the case of disabled IRQs and a pending IRQ directly simulate an
      // external interrupt intercept.
      Vmx_vm_entry_interrupt_info int_info;
      int_info.value = vm_state->read<Vmx::Vmcs_vm_entry_interrupt_info>();

      bool valid = int_info.valid();
      bool immediate_exit = int_info.immediate_exit();

      if (immediate_exit)
        {
          // Zero the Fiasco-specific bit
          int_info.immediate_exit() = 0;
          vm_state->write<Vmx::Vmcs_vm_entry_interrupt_info>(int_info.value);

          // We have a request for immediate exit after VM exit after the
          // interrupt injection. To achieve this we trigger a self-IPI with
          // closed interrupts which will lead to an immediate VM exit after
          // the event injection.
          // NOTE: It is possible to use the preemption timer for this
          // NOTE: However, the timer is broken on many CPUs or may not
          // NOTE: be supported at all. So keep it simple for now.
          assert(cpu_lock.test());
          Ipi::send(Ipi::Global_request, current_cpu(), current_cpu());
        }
      else if (   !(vcpu->_saved_state & Vcpu_state::F_irqs)
               && (vcpu->sticky_flags & Vcpu_state::Sf_irq_pending))
        {
          // When injection did not yet occur (the valid bit in the interrupt
          // information field is still 1), tell the VMM we were interrupted
          // during event delivery
          if (valid)
            {
              vm_state->write<Vmx::Vmcs_idt_vectoring_info>(int_info.value);
              vm_state->write<Vmx::Vmcs_vm_exit_insn_length>
                               (vm_state->read<Vmx::Vmcs_vm_entry_insn_len>());

              if (int_info.deliver_error_code())
                vm_state->write<Vmx::Vmcs_idt_vectoring_error>
                                 (vm_state->read<Vmx::Vmcs_vm_entry_exception_error>());

              // Invalidate VM-entry interrupt information
              int_info.valid() = 0;
              vm_state->write<Vmx::Vmcs_vm_entry_interrupt_info>(int_info.value);
            }

          // XXX: check if this is correct, we set external irq exit as reason
          vm_state->write<Vmx::Vmcs_exit_reason>(Exit_irq);
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          return 1; // return 1 to indicate pending IRQs (IPCs)
        }

      int r = do_resume_vcpu(ctxt, vcpu, vm_state);

      // test for error or non-IRQ exit reason
      if (r <= 0 || immediate_exit)
        {
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          force_kern_entry_vcpu_state(vcpu);
          return r;
        }

      // check for IRQ exits and allow to handle the IRQ
      if (r == 1)
        Proc::preemption_point();

      // Check if the current context got a message delivered.
      // This is done by testing for a valid continuation.
      // When a continuation is set we have to directly
      // leave the kernel to not overwrite the vcpu-regs
      // with bogus state.
      Thread *t = nonull_static_cast<Thread *>(ctxt);
      if (t->continuation_test_and_restore())
        {
          force_kern_entry_vcpu_state(vcpu);
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          t->vcpu_return_to_kernel(vcpu->_entry_ip, vcpu->_entry_sp,
                                   t->vcpu_state().usr().get());
        }
    }
}

PUBLIC inline template<typename VARIANT>
void
Vm_vmx_t<VARIANT>::cleanup_vcpu(Context *ctxt, Vcpu_state *vcpu) override
{
  if (EXPECT_FALSE(!(ctxt->state(true) & Thread_ext_vcpu_enabled)))
    return;

  Vmx_vm_state *vm_state
    = offset_cast<Vmx_vm_state *>(vcpu, Config::Ext_vcpu_state_offset);
  vm_state->clear_vmcs(ctxt);
}

//------------------------------------------------------------------
IMPLEMENTATION [vmx && amd64]:

PRIVATE inline template<typename VARIANT>
void
Vm_vmx_t<VARIANT>::safe_host_segments()
{
  /* we can optimize GS and FS handling based on the assuption that
   * FS and GS do not change often for the host / VMM */
  Vmx::vmcs_write<Vmx::Vmcs_host_fs_base>(Cpu::rdmsr(Msr::Ia32_fs_base));
  Vmx::vmcs_write<Vmx::Vmcs_host_gs_base>(Cpu::rdmsr(Msr::Ia32_gs_base));
  Vmx::vmcs_write<Vmx::Vmcs_host_fs_selector>(Cpu::get_fs());
  Vmx::vmcs_write<Vmx::Vmcs_host_gs_selector>(Cpu::get_gs());
}

/**
 * Store certain long-mode guest MSRs to the software VMCS and setup host MSRs
 * values.
 *
 * In case the guest is currently not running in long mode, the MSRs are not
 * stored (and there is also no need to reinitialize the host MSRs values).
 *
 * \param vm_state         Software VMCS.
 * \param guest_long_mode  Guest running in long mode.
 */
PRIVATE inline template<typename VARIANT>
void
Vm_vmx_t<VARIANT>::store_guest_restore_host_msrs(Vmx_vm_state *vm_state,
                                                 bool guest_long_mode)
{
  // Nothing needs to be done if the guest is not running in long mode.
  if (!guest_long_mode)
    return;

  vm_state->write<Vmx::Sw_msr_syscall_mask>(Cpu::rdmsr(Msr::Ia32_fmask));
  vm_state->write<Vmx::Sw_msr_cstar>(Cpu::rdmsr(Msr::Ia32_cstar));
  vm_state->write<Vmx::Sw_msr_lstar>(Cpu::rdmsr(Msr::Ia32_lstar));
  vm_state->write<Vmx::Sw_msr_star>(Cpu::rdmsr(Msr::Ia32_star));
  // Msr::Tsc_aux
  vm_state->write<Vmx::Sw_msr_kernel_gs_base>(Cpu::rdmsr(Msr::Ia32_kernel_gs_base));

  // setup_sysenter sets SFMASK, CSTAR, LSTAR and STAR MSR
  Cpu::cpus.current().setup_sysenter();
  // Msr::Tsc_aux
  // Reset msr_kernel_gs_base to avoid possible inter-vm channel
  Cpu::wrmsr(0UL, Msr::Ia32_kernel_gs_base);
}

/**
 * Load certain long-mode guest MSRs from the software VMCS.
 *
 * In case the guest is currently not running in long mode, the MSRs are not
 * loaded.
 *
 * \param vm_state         Software VMCS.
 * \param guest_long_mode  Guest running in long mode.
 */
PRIVATE inline template<typename VARIANT>
void
Vm_vmx_t<VARIANT>::load_guest_msrs(Vmx_vm_state const *vm_state,
                                   bool guest_long_mode)
{
  // Nothing needs to be done if the guest is not running in long mode.
  if (!guest_long_mode)
    return;

  // Writing to CSTAR, LSTAR or KERNEL_GS_BASE triggers a #GP fault if the
  // provided address is not canonical.
  Cpu::wrmsr(vm_state->read<Vmx::Sw_msr_syscall_mask>(), Msr::Ia32_fmask);
  Cpu::set_canonical_msr(vm_state->read<Vmx::Sw_msr_cstar>(), Msr::Ia32_cstar);
  Cpu::set_canonical_msr(vm_state->read<Vmx::Sw_msr_lstar>(), Msr::Ia32_lstar);
  Cpu::wrmsr(vm_state->read<Vmx::Sw_msr_star>(), Msr::Ia32_star);
  // Msr::Tsc_aux
  Cpu::set_canonical_msr(vm_state->read<Vmx::Sw_msr_kernel_gs_base>(),
                         Msr::Ia32_kernel_gs_base);
}

//------------------------------------------------------------------
IMPLEMENTATION [vmx && !amd64]:

PRIVATE inline template<typename VARIANT>
void
Vm_vmx_t<VARIANT>::safe_host_segments()
{
  /* GS and FS are handled via push/pop in asm code */
}

PRIVATE inline template<typename VARIANT>
void
Vm_vmx_t<VARIANT>::store_guest_restore_host_msrs(Vmx_vm_state *, bool)
{}

PRIVATE inline template<typename VARIANT>
void
Vm_vmx_t<VARIANT>::load_guest_msrs(Vmx_vm_state *, bool)
{}
