IMPLEMENTATION [mips]:

PRIVATE virtual
bool
Task::invoke_arch(L4_msg_tag &, Utcb *)
{
  return false;
}


//----------------------------------------------------------------
IMPLEMENTATION [mips_vz]:

#include "static_init.h"
#include "l4_types.h"

extern "C" void vcpu_resume(Trap_state *, Return_frame *sp)
   FIASCO_FASTCALL FIASCO_NORETURN;

IMPLEMENT_OVERRIDE
int
Task::resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode)
{
  Trap_state ts;
  ctxt->copy_and_sanitize_trap_state(&ts, &vcpu->_regs.s);

  // prevent VZ guest entry with a normal Task
  if (EXPECT_FALSE(ts.status & (1 << 3)))
    return -L4_err::EInval;

  if (user_mode)
    {
      ctxt->state_add_dirty(Thread_vcpu_user);
      vcpu->state |= Vcpu_state::F_traps | Vcpu_state::F_exceptions
                     | Vcpu_state::F_debug_exc;

      ctxt->space_ref()->user_mode(user_mode);

      ctxt->vcpu_pv_switch_to_user(vcpu, true);
      switchin_context(ctxt->space());
    }

  vcpu_resume(&ts, ctxt->regs());
}

class Vz_vm : public Task
{
public:
  explicit Vz_vm(Ram_quota *q)
  : Task(q, Caps::mem() | Caps::obj())
  { _is_vz_guest = true; }
};

PUBLIC
int
Vz_vm::resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode) override
{
  // prevent non-VZ execution in VZ VM
  if (EXPECT_FALSE(!user_mode))
    return -L4_err::EInval;

  Trap_state ts;
  ctxt->copy_and_sanitize_trap_state(&ts, &vcpu->_regs.s);

  // prevent non-VZ execution in VZ VM
  // test for guest mode. Note, using ts->status is ok since
  // Thread_ext_vcpu_enabled is checked by copy_and_sanitize_trap_state
  // already and reflected in ts->status
  if (EXPECT_FALSE(!(ts.status & (1 << 3))))
    return -L4_err::EInval;

  ctxt->state_add_dirty(Thread_vcpu_user);
  vcpu->state |= Vcpu_state::F_traps | Vcpu_state::F_exceptions
                 | Vcpu_state::F_debug_exc;

  ctxt->space_ref()->user_mode(user_mode);
  auto &owner = Vz::owner.cpu(ctxt->get_current_cpu());
  if (owner.ctxt != ctxt)
    {
      if (owner.ctxt)
        owner.ctxt->vz_save_state(owner.guest_id);

      owner.ctxt = ctxt;
      owner.guest_id = switchin_guest_context();
      ctxt->vz_load_state(owner.guest_id);
    }
  else
    {
      auto s = ctxt->vm_state(vcpu);
      owner.guest_id = switchin_guest_context();
      s->load_selective(owner.guest_id);
      // assume all cp0 context to be dirty (modified by the guest)
      write_now(&s->current_cp0_map, 0);
    }

  vcpu_resume(&ts, ctxt->regs());
}

namespace {

static inline void
mips_init_vz_factory()
{
  if (!Cpu::options.vz())
    return;

  Kobject_iface::set_factory(L4_msg_tag::Label_vm,
                             &Task::generic_factory<Vz_vm>);
}

STATIC_INITIALIZER(mips_init_vz_factory);

} // annon namespace

