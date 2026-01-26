IMPLEMENTATION [riscv]:

PRIVATE inline
bool
Task::invoke_arch(L4_msg_tag &, Utcb *)
{
  return false;
}

extern "C" [[noreturn]] void vcpu_resume(Trap_state *, Return_frame *sp)
   FIASCO_FASTCALL;

IMPLEMENT_OVERRIDE
int
Task::resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode)
{
  assert(cpu_lock.test());

  // Prevent execution in regular task with virtualization enabled context.
  if (EXPECT_FALSE(ctxt->state(true) & Thread_ext_vcpu_enabled))
      return -L4_err::EInval;

  if (user_mode)
    {
      ctxt->vcpu_save_host_regs(vcpu);
      ctxt->state_add_dirty(Thread_vcpu_user);
      vcpu->state |= Vcpu_state::F_traps;
    }
  else
    // When resuming to vCPU host mode populate the host mode preserved
    // registers in the vCPU state with their current values.
    ctxt->vcpu_copy_host_regs(vcpu);

  Trap_state ts;
  ctxt->copy_and_sanitize_trap_state(&ts, &vcpu->_regs.s);

  ctxt->space_ref()->user_mode(user_mode);
  switchin_context(ctxt->space());
  vcpu_resume(&ts, ctxt->regs());
}

//----------------------------------------------------------------
IMPLEMENTATION [riscv && cpu_virt]:

static Kmem_slab_t<Vm> _vm_allocator("Vm");

class Vm : public Task
{
public:
  explicit Vm(Ram_quota *q)
  : Task(q, Caps::mem() | Caps::obj())
  {
    _tlb_type = Mem_unit::Have_vmids ? Tlb_per_cpu_asid : Tlb_per_cpu_global;
  }

private:
  // G-stage page dirs are four times the size of a regular page directory.
  using Gdir_alloc = Kmem_slab_s<sizeof(Dir_type) * 4, sizeof(Dir_type) * 4>;
  static Gdir_alloc _gdir_alloc;
};

Vm::Gdir_alloc Vm::_gdir_alloc;

PUBLIC virtual bool Task::is_vm() const { return false; }
PUBLIC bool Vm::is_vm() const override { return true; }

PUBLIC static
Vm *Vm::alloc(Ram_quota *q)
{
  return _vm_allocator.q_new(q, q);
}

PUBLIC inline
void *
Vm::operator new([[maybe_unused]] size_t size, void *p) noexcept
{
  assert (size == sizeof(Vm));
  return p;
}

PUBLIC
void
Vm::operator delete(Vm *vm, std::destroying_delete_t)
{
  Ram_quota *q = vm->ram_quota();
  vm->~Vm();
  _vm_allocator.q_free(q, vm);
}

PROTECTED inline
Mem_space::Dir_type *
Vm::alloc_dir() override
{
  // The hypervisor extensions g-stage address translation supports
  // guest physical address spaces four times the size as the host's
  // virtual address space.
  // Fiasco currently only makes use of the lower quarter
  // of the guest physical address space.
  Dir_type *dir = static_cast<Dir_type *>(_gdir_alloc.q_alloc(ram_quota()));
  if (dir)
    {
      // Clear the whole guest physical address space.
      for (unsigned i = 0; i < 4; i++)
        (dir + i)->clear(true);
    }
  return dir;
}

PROTECTED inline
void
Vm::free_dir(Mem_space::Dir_type *dir) override
{
  _gdir_alloc.q_free(ram_quota(), dir);
}

PUBLIC
void
Vm::tlb_flush_space(bool with_vmid) override
{
  if (!Mem_unit::Have_vmids || (with_vmid && c_vmid() != Mem_unit::Vmid_invalid))
    {
      Mem_unit::hfence_gvma_vmid(c_vmid());
      if (_current_guest.current() != this)
        tlb_mark_unused();
    }
}

PUBLIC
void
Vm::tlb_flush_on_sync() override
{
  Mem_unit::hfence_gvma_vmid(vmid());
}

PUBLIC
int
Vm::resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode) override
{
  assert(cpu_lock.test());

  // Prevent non-virtualization mode execution in VMM task.
  if (EXPECT_FALSE(!user_mode))
    return -L4_err::EInval;

  // Prevent execution in VM task with non-virtualization enabled context.
  if (EXPECT_FALSE(!(ctxt->state(true) & Thread_ext_vcpu_enabled)))
    return -L4_err::EInval;

  // Switching from vCPU host mode to user mode.
  assert(!(ctxt->state() & Thread_vcpu_user));
  ctxt->vcpu_save_host_regs(vcpu);

  ctxt->state_add_dirty(Thread_vcpu_user);
  vcpu->state |= Vcpu_state::F_traps;

  Trap_state ts;
  ctxt->copy_and_sanitize_trap_state(&ts, &vcpu->_regs.s);
  ts.virt_mode(true);

  // We intentionally do not set the space of the vCPU to user mode, as the VM page
  // table is managed in its own separate control register.
  switchin_guest_space();

  // Load hypervisor extension state
  Context::vm_state(vcpu)->load();

  // TODO: Remote fences should be implemented by the VMM in user mode.
  Context::vm_state(vcpu)->process_remote_fence();

  vcpu_resume(&ts, ctxt->regs());
}

static inline void
init_he_factory()
{
  if (!Cpu::has_isa_ext(Cpu::Isa_ext_h))
    return;

  Kobject_iface::set_factory(L4_msg_tag::Label_vm,
                             &Task::generic_factory<Vm, false>);
}

STATIC_INITIALIZER(init_he_factory);
