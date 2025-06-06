INTERFACE [vmx]:

#include "ptab_base.h"
#include "tlbs.h"
#include "vm.h"
#include "vmx.h"

class Vm_vmx_ept_tlb : public Tlb
{
public:
  enum Context
  {
    Single = 0x1,
    Global = 0x2,
  };

  static void invept(Context type, Mword eptp = 0)
  {
    struct Op
    {
      Unsigned64 eptp, resv;
    } op = {eptp, 0};

    asm volatile (
      "invept %[op], %[type]\n"
      :
      : [op] "m" (op),
        [type] "r" (static_cast<Mword>(type))
      : "cc"
    );
  }

  /**
   * Flush only the TLB entries corresponding to the given EPTP.
   */
  static void flush_single(Mword eptp)
  {
    if (Vmx::cpus.current().info.has_invept_single())
      invept(Single, eptp);
    else
      invept(Global);
  }

  /**
   * Flush the complete EPT TLB.
   */
  static void flush()
  {
    invept(Global);
  }

  explicit Vm_vmx_ept_tlb(Cpu_number cpu)
  {
    auto const &vmx = Vmx::cpus.cpu(cpu);
    if (!vmx.vmx_enabled())
      return;

    // Clean out residual EPT TLB entries during boot.
    Vm_vmx_ept_tlb::flush();

    register_cpu_tlb(cpu);
  }

  inline void tlb_flush() override
  {
    Vm_vmx_ept_tlb::flush();
  }

private:
  static Per_cpu<Vm_vmx_ept_tlb> cpu_tlbs;
};

/**
 * VMX implementation variant with EPT support.
 */
class Vm_vmx_ept : public cxx::Dyn_castable<Vm_vmx_ept, Vm>
{
private:
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

  class Epte_ptr
  {
    Unsigned64 *e;

    void set(Unsigned64 v);

  public:
    typedef Mem_space::Attr Attr;

    unsigned char level;

    Epte_ptr() = default;
    Epte_ptr(Unsigned64 *e, unsigned char level) : e(e), level(level) {}

    bool is_valid() const { return *e & 7; }
    bool is_leaf() const { return (*e & (1 << 7)) || level == 3; }
    Unsigned64 next_level() const
    { return cxx::get_lsb(cxx::mask_lsb(*e, 12), 52); }

    void clear() { set(0); }

    unsigned page_level() const;
    unsigned char page_order() const;
    Unsigned64 page_addr() const;

    Attr attribs() const
    {
      auto raw = access_once(e);

      Page::Rights r = Page::Rights::UR();
      if (raw & 2) r |= Page::Rights::W();
      if (raw & 4) r |= Page::Rights::X();

      Page::Type t;
      switch (raw & 0x38)
        {
        case (0 << 3): t = Page::Type::Uncached(); break;
        case (1 << 3): t = Page::Type::Buffered(); break;
        default:
        case (6 << 3): t = Page::Type::Normal(); break;
        }

      return Attr(r, t, Page::Kern::None(), Page::Flags::None());
    }

    Unsigned64 entry() const { return *e; }

    void set_next_level(Unsigned64 phys)
    { set(phys | 7); }

    void write_back_if(bool) const {}
    static void write_back(void *, void*) {}

    Page::Flags access_flags() const
    {
      return Page::Flags::None();
    }

    void del_rights(Page::Rights r)
    {
      Unsigned64 dr = 0;

      if (r & Page::Rights::W())
        dr = 2;

      if (r & Page::Rights::X())
        dr |= 4;

      if (dr)
        *e &= ~dr;
    }

    Unsigned64 make_page(Phys_mem_addr addr, Page::Attr attr)
    {
      Unsigned64 r = (level < 3) ? 1ULL << 7 : 0ULL;
      r |= 1; // R
      if (attr.rights & Page::Rights::W()) r |= 2;
      if (attr.rights & Page::Rights::X()) r |= 4;

      if (attr.type == Page::Type::Normal())   r |= 6 << 3;
      if (attr.type == Page::Type::Buffered()) r |= 1 << 3;
      if (attr.type == Page::Type::Uncached()) r |= 0;

      return cxx::int_value<Phys_mem_addr>(addr) | r;
    }

    void set_page(Unsigned64 p)
    {
      set(p);
    }
  };

  typedef Ptab::Tupel<Ptab::Traits<Unsigned64, 39, 9, false>,
                      Ptab::Traits<Unsigned64, 30, 9, true>,
                      Ptab::Traits<Unsigned64, 21, 9, true>,
                      Ptab::Traits<Unsigned64, 12, 9, true>>::List Ept_traits;

  typedef Ptab::Shift<Ept_traits, 12>::List Ept_traits_vpn;
  typedef Ptab::Page_addr_wrap<Page_number, 12> Ept_va_vpn;
  typedef Ptab::Base<Epte_ptr, Ept_traits_vpn, Ept_va_vpn, Mem_layout> Ept;

  Mword _eptp;
  Ept *_ept;
};

// -------------------------------------------------------------------------
IMPLEMENTATION [obj_space_virt]:

// avoid warnings about hiding virtual functions from Obj_space_phys_override
EXTENSION class Vm_vmx_ept
{
private:
  using Vm::v_lookup;
  using Vm::v_delete;
  using Vm::v_insert;
};

// -------------------------------------------------------------------------
IMPLEMENTATION [vmx && 64bit]:

IMPLEMENT inline
void
Vm_vmx_ept::Epte_ptr::set(Unsigned64 v) { write_now(e, v); }

// -------------------------------------------------------------------------
IMPLEMENTATION [vmx && !64bit]:

IMPLEMENT inline
void
Vm_vmx_ept::Epte_ptr::set(Unsigned64 v)
{
  // this assumes little endian!
  union T
  {
    Unsigned64 l;
    Unsigned32 u[2];
  };

  T *t = reinterpret_cast<T*>(e);

  write_now(&t->u[0], 0U);
  write_now(&t->u[1], static_cast<Unsigned32>(v >> 32));
  write_now(&t->u[0], static_cast<Unsigned32>(v));
}

// -------------------------------------------------------------------------
IMPLEMENTATION [vmx]:

#include "context.h"
#include "ipi.h"
#include "thread.h"

static Kmem_slab_t<Vm_vmx_ept> _ept_allocator("Vm_vmx_ept");

JDB_DEFINE_TYPENAME(Vm_vmx_ept, "\033[33;1mVm\033[m");

IMPLEMENT inline
unsigned
Vm_vmx_ept::Epte_ptr::page_level() const
{ return level; }

IMPLEMENT inline
unsigned char
Vm_vmx_ept::Epte_ptr::page_order() const
{ return Vm_vmx_ept::Ept::page_order_for_level(level); }

IMPLEMENT inline
Unsigned64
Vm_vmx_ept::Epte_ptr::page_addr() const
{ return cxx::get_lsb(cxx::mask_lsb(*e, Vm_vmx_ept::Ept::page_order_for_level(level)), 52); }

static Mem_space::Fit_size __ept_ps;

PUBLIC
Mem_space::Fit_size const &
Vm_vmx_ept::mem_space_fitting_sizes() const override
{ return __ept_ps; }

PUBLIC static
void
Vm_vmx_ept::add_page_size(Mem_space::Page_order o)
{
  add_global_page_size(o);
  __ept_ps.add_page_size(o);
}

PUBLIC
void
Vm_vmx_ept::tlb_flush_current_cpu() override
{
  Vm_vmx_ept_tlb::flush_single(_eptp);
  tlb_mark_unused();
}

PUBLIC
bool
Vm_vmx_ept::v_lookup(Mem_space::Vaddr virt, Mem_space::Phys_addr *phys,
                     Mem_space::Page_order *order,
                     Mem_space::Attr *page_attribs) override
{
  auto i = _ept->walk(virt);
  if (order) *order = Mem_space::Page_order(i.page_order());

  if (!i.is_valid())
    return false;

  // FIXME: we may get a problem on 32 bit systems and using more than 4G ram
  if (phys) *phys = Mem_space::Phys_addr(i.page_addr());
  if (page_attribs) *page_attribs = i.attribs();

  return true;
}

PUBLIC
Mem_space::Status
Vm_vmx_ept::v_insert(Mem_space::Phys_addr phys, Mem_space::Vaddr virt,
                     Mem_space::Page_order order,
                     Mem_space::Attr page_attribs, bool) override
{
  // insert page into page table

  // XXX should modify page table using compare-and-swap

  assert(cxx::is_zero(cxx::get_lsb(Mem_space::Phys_addr(phys), order)));
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  int level;
  for (level = 0; level <= Ept::Depth; ++level)
    if (Mem_space::Page_order(Ept::page_order_for_level(level)) <= order)
      break;

  auto i = _ept->walk(virt, level, false,
                      Kmem_alloc::q_allocator(ram_quota()));

  if (EXPECT_FALSE(!i.is_valid() && i.level != level))
    return Mem_space::Insert_err_nomem;

  if (EXPECT_FALSE(i.is_valid()
                   && (i.level != level || Mem_space::Phys_addr(i.page_addr()) != phys)))
    return Mem_space::Insert_err_exists;

  auto entry = i.make_page(phys, page_attribs);

  if (i.is_valid())
    {
      if (EXPECT_FALSE(i.entry() == entry))
        return Mem_space::Insert_warn_exists;

      i.set_page(entry);
      return Mem_space::Insert_warn_attrib_upgrade;
    }
  else
    {
      i.set_page(entry);
      return Mem_space::Insert_ok;
    }
}

PUBLIC
Page::Flags
Vm_vmx_ept::v_delete(Mem_space::Vaddr virt,
                     [[maybe_unused]] Mem_space::Page_order order,
                     Page::Rights rights) override
{
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  auto pte = _ept->walk(virt);

  if (EXPECT_FALSE(!pte.is_valid()))
    return Page::Flags::None();

  Page::Flags flags = pte.access_flags();

  if (!(rights & Page::Rights::R()))
    {
      // downgrade PDE (superpage) rights
      pte.del_rights(rights);
    }
  else
    {
      // delete PDE (superpage)
      pte.clear();
    }

  return flags;
}

PUBLIC
void
Vm_vmx_ept::v_add_access_flags(Mem_space::Vaddr, Page::Flags) override
{}

PUBLIC static
Vm_vmx_ept *Vm_vmx_ept::alloc(Ram_quota *q)
{
  return _ept_allocator.q_new(q, q);
}

PUBLIC inline
void *
Vm_vmx_ept::operator new (size_t size, void *p) noexcept
{
  (void)size;
  assert(size == sizeof(Vm_vmx_ept));
  return p;
}

PUBLIC
void
Vm_vmx_ept::operator delete (Vm_vmx_ept *vm, std::destroying_delete_t)
{
  Ram_quota *q = vm->ram_quota();
  vm->~Vm_vmx_ept();
  _ept_allocator.q_free(q, vm);
}

PUBLIC inline
Vm_vmx_ept::Vm_vmx_ept(Ram_quota *q) : Dyn_castable_class(q)
{
  _tlb_type = Tlb_per_cpu_asid;
}

PUBLIC
Vm_vmx_ept::~Vm_vmx_ept()
{
  if (_ept)
    {
      _ept->destroy(Virt_addr(0UL), Virt_addr(~0UL), 0, Ept::Depth,
                    Kmem_alloc::q_allocator(ram_quota()));
      Kmem_alloc::allocator()->q_free(ram_quota(), Config::page_order(), _ept);
      _ept = nullptr;
      _eptp = 0;
    }
}

PUBLIC inline
void
Vm_vmx_ept::to_vmcs()
{
  Vmx::vmcs_write<Vmx::Vmcs_ept_pointer>(_eptp);
  tlb_mark_used();
}

PUBLIC inline
bool
Vm_vmx_ept::initialize()
{
  void *b;
  if (EXPECT_FALSE(!(b = Kmem_alloc::allocator()
	  ->q_alloc(ram_quota(), Config::page_order()))))
    return false;

  _ept = static_cast<Ept*>(b);
  _ept->clear(false);	// initialize to zero
  _eptp = Mem_layout::pmem_to_phys(_ept) | 6 | (3 << 3);
  return true; // success

}

PUBLIC inline
void
Vm_vmx_ept::load_vm_memory(Vmx_vm_state *vm_state)
{
  vm_state->load_cr3();
  to_vmcs();
}

PUBLIC inline
void
Vm_vmx_ept::store_vm_memory(Vmx_vm_state *vm_state)
{
  vm_state->store_cr3();
}

PUBLIC static
void
Vm_vmx_ept::init()
{
  auto const &vmx = Vmx::cpus.cpu(Cpu_number::boot_cpu());
  if (!vmx.vmx_enabled())
    return;

  Kobject_iface::set_factory(L4_msg_tag::Label_vm,
                             Task::generic_factory<Vm_vmx_ept>);

  printf("VMX: init page sizes\n");
  add_page_size(Mem_space::Page_order(12));
  add_page_size(Mem_space::Page_order(21));
  add_page_size(Mem_space::Page_order(30));
}

PRIVATE inline NEEDS[Vm_vmx_ept::safe_host_segments,
                     Vm_vmx_ept::load_guest_msrs,
                     Vm_vmx_ept::store_guest_restore_host_msrs]
template<Host_state HOST_STATE>
unsigned long
Vm_vmx_ept::vm_entry(Trex *regs, Vmx_vm_state_t<HOST_STATE> *vm_state,
                     bool guest_long_mode, Unsigned32 *reason)
{
  Cpu &cpu = Cpu::cpus.current();
  Vmx &vmx = Vmx::cpus.current();

  vm_state->load_guest_state();
  load_vm_memory(vm_state);

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

  if (vmx.info.exit_ctls.allowed(Vmx_info::Ex_load_perf_global_ctl)
      && cpu.arch_perf_mon_version() > 0)
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
  store_vm_memory(vm_state);
  vm_state->store_exit_info(error, *reason);

  return ret;
}

PRIVATE inline
int
Vm_vmx_ept::do_resume_vcpu(Context *ctxt, Vcpu_state *vcpu,
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

  if constexpr (TAG_ENABLED(lazy_fpu))
    {
      // XXX: This generates a circular dependency between thread<->task!
      if (!(ctxt->state() & Thread_fpu_owner))
        {
          if (EXPECT_FALSE(!static_cast<Thread *>(ctxt)->switchin_fpu()))
            {
              WARN("VMX: switchin_fpu failed\n");
              return -L4_err::EInval;
            }
        }
    }

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

PUBLIC inline
int
Vm_vmx_ept::resume_vcpu(Context *ctxt, Vcpu_state *vcpu,
                        [[maybe_unused]] bool user_mode) override
{
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

PUBLIC inline
void
Vm_vmx_ept::cleanup_vcpu(Context *ctxt, Vcpu_state *vcpu) override
{
  if (EXPECT_FALSE(!(ctxt->state(true) & Thread_ext_vcpu_enabled)))
    return;

  Vmx_vm_state *vm_state
    = offset_cast<Vmx_vm_state *>(vcpu, Config::Ext_vcpu_state_offset);
  vm_state->clear_vmcs(ctxt);
}

STATIC_INITIALIZE(Vm_vmx_ept);

//------------------------------------------------------------------
IMPLEMENTATION [vmx && amd64]:

PRIVATE inline
void
Vm_vmx_ept::safe_host_segments()
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
PRIVATE inline
void
Vm_vmx_ept::store_guest_restore_host_msrs(Vmx_vm_state *vm_state,
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
PRIVATE inline
void
Vm_vmx_ept::load_guest_msrs(Vmx_vm_state const *vm_state, bool guest_long_mode)
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

PRIVATE inline
void
Vm_vmx_ept::safe_host_segments()
{
  /* GS and FS are handled via push/pop in asm code */
}

PRIVATE inline
void
Vm_vmx_ept::store_guest_restore_host_msrs(Vmx_vm_state *, bool)
{}

PRIVATE inline
void
Vm_vmx_ept::load_guest_msrs(Vmx_vm_state *, bool)
{}

// -------------------------------------------------------------------------
IMPLEMENTATION[vmx]:

// Must be constructed after Vmx::cpus.
DEFINE_PER_CPU_LATE Per_cpu<Vm_vmx_ept_tlb>
  Vm_vmx_ept_tlb::cpu_tlbs(Per_cpu_data::Cpu_num);
