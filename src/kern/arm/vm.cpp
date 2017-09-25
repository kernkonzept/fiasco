INTERFACE [arm_em_tz]:

#include "task.h"

class Vm : public cxx::Dyn_castable<Vm, Task>
{
public:
  explicit Vm(Ram_quota *q)
    : Dyn_castable_class(q, Caps::mem() | Caps::obj()) {}

  struct Vm_state_mode
  {
    Mword sp;
    Mword lr;
    Mword spsr;
  };

  enum
  {
    Max_num_inject_irqs = 32 * 8,
    Nr_irq_fields = (Max_num_inject_irqs + (sizeof(Unsigned32) * 8 - 1))
                    / (sizeof(Unsigned32) * 8),
  };

  struct Vm_state_irq_inject
  {
    Unsigned32 group;
    Unsigned32 irqs[Nr_irq_fields];
  };

  struct Vm_state
  {
    Mword r[13];

    Mword sp_usr;
    Mword lr_usr;

    Vm_state_mode irq;
    Mword r_fiq[5]; // r8 - r12
    Vm_state_mode fiq;
    Vm_state_mode abt;
    Vm_state_mode und;
    Vm_state_mode svc;

    Mword pc;
    Mword cpsr;

    Mword pending_events;
    Unsigned32 cpacr;
    Mword cp10_fpexc;

    Mword pfs;
    Mword pfa;
    Mword exit_reason;

    Vm_state_irq_inject irq_inject;
  };

private:
  Vm_state *state_for_dbg;

  enum Exit_reasons
  {
    ER_none       = 0,
    ER_vmm_call   = 1,
    ER_inst_abort = 2,
    ER_data_abort = 3,
    ER_irq        = 4,
    ER_fiq        = 5,
    ER_undef      = 6,
  };

};

//-----------------------------------------------------------------------------
INTERFACE [debug && arm_em_tz]:

#include "tb_entry.h"

EXTENSION class Vm
{
public:
  struct Vm_log : public Tb_entry
  {
    bool is_entry;
    Mword pc;
    Mword cpsr;
    Mword exit_reason;
    Mword pending_events;
    Mword r0;
    Mword r1;
    void print(String_buffer *buf) const;
 };
};

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "cpu.h"
#include "cpu_lock.h"
#include "entry_frame.h"
#include "ipc_timeout.h"
#include "logdefs.h"
#include "mem_space.h"
#include "thread_state.h"
#include "timer.h"
#include "kmem_slab.h"

JDB_DEFINE_TYPENAME(Vm, "\033[33;1mVm\033[m");

PUBLIC inline virtual
Page_number
Vm::map_max_address() const
{ return Page_number(1UL << (MWORD_BITS - Mem_space::Page_shift)); }

PUBLIC inline
void *
Vm::operator new(size_t size, void *p) throw()
{
  (void)size;
  assert (size == sizeof(Vm));
  return p;
}

PUBLIC
void
Vm::operator delete(void *ptr)
{
  Vm *t = reinterpret_cast<Vm*>(ptr);
  Kmem_slab_t<Vm>::q_free(t->ram_quota(), ptr);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_em_tz]:

#include "pic.h"
#include "thread.h"

PUBLIC
int
Vm::resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode)
{
  (void)user_mode;
  assert(user_mode);

  assert(cpu_lock.test());

  if (EXPECT_FALSE(!(ctxt->state(true) & Thread_ext_vcpu_enabled)))
    {
      ctxt->arch_load_vcpu_kern_state(vcpu, true);
      return -L4_err::EInval;
    }

  Vm_state *state = reinterpret_cast<Vm_state *>(reinterpret_cast<char *>(vcpu) + 0x400);

  state_for_dbg = state;

  if (state->irq_inject.group)
    {
      Unsigned32 g = state->irq_inject.group;
      for (unsigned i = 1; i < Nr_irq_fields; ++i)
        if ((1 << i) & g)
          {
            Pic::set_pending_irq(i, state->irq_inject.irqs[i]);
            state->irq_inject.irqs[i] = 0;
          }

      state->irq_inject.group = 0;
    }

  while (true)
    {
      if (   !(vcpu->_saved_state & Vcpu_state::F_irqs)
          && (vcpu->sticky_flags & Vcpu_state::Sf_irq_pending))
        {
          state->exit_reason = ER_irq;
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          return 1;
        }

      log_vm(state, 1);

      if (!get_fpu())
        {
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          return -L4_err::EInval;
        }

      Cpu::cpus.current().tz_switch_to_ns((Mword *)state);

      assert(cpu_lock.test());

      log_vm(state, 0);

      if (state->exit_reason == ER_irq || state->exit_reason == ER_fiq)
        Proc::preemption_point();

      switch (state->exit_reason)
        {
        case ER_data_abort:
          if ((state->pfs & 0x237) == 0x211)
            break;
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          return 0;
        case ER_undef:
          printf("should not happen: %lx\n", state->pc);
          // fall through
        case ER_vmm_call:
        case ER_inst_abort:
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          return 0;
        }

      Thread *t = nonull_static_cast<Thread*>(ctxt);
      if (t->continuation_test_and_restore())
        {
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          t->fast_return_to_user(vcpu->_entry_ip, vcpu->_entry_sp,
                                 t->vcpu_state().usr().get());
        }
    }
}

namespace {
static Kobject_iface * FIASCO_FLATTEN
vm_factory(Ram_quota *q, Space *,
           L4_msg_tag, Utcb const *,
           int *err)
{
  typedef Kmem_slab_t<Vm> Alloc;

  *err = L4_err::ENomem;
  Vm *v = Alloc::q_new(q, q);

  if (EXPECT_FALSE(!v))
    return 0;

  if (EXPECT_TRUE(v->initialize()))
    return v;

  Alloc::q_del(q, v);
  return 0;
}

static inline void __attribute__((constructor)) FIASCO_INIT
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_vm, vm_factory);
}
}

// --------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_em_tz && fpu]:

PUBLIC
bool
Vm::get_fpu()
{
  if (!(current()->state() & Thread_fpu_owner))
    {
      if (!current_thread()->switchin_fpu())
        {
          printf("tz: switchin_fpu failed\n");
          return false;
        }
    }
  return true;
}

// --------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_em_tz && !fpu]:

PUBLIC
bool
Vm::get_fpu()
{ return true; }

// --------------------------------------------------------------------------
IMPLEMENTATION [debug && arm_em_tz]:

#include "jdb.h"
#include "string_buffer.h"

PRIVATE
Mword
Vm::jdb_get(Mword *state_ptr)
{
  Mword v = ~0UL;
  Jdb::peek(state_ptr, this, v);
  return v;
}

PUBLIC
void
Vm::dump_vm_state()
{
  Vm_state *s = state_for_dbg;
  printf("pc: %08lx  cpsr: %08lx exit_reason:%ld \n",
         jdb_get(&s->pc), jdb_get(&s->cpsr), jdb_get(&s->exit_reason));
  printf("r0: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
         jdb_get(&s->r[0]), jdb_get(&s->r[1]), jdb_get(&s->r[2]), jdb_get(&s->r[3]),
         jdb_get(&s->r[4]), jdb_get(&s->r[5]), jdb_get(&s->r[6]), jdb_get(&s->r[7]));
  printf("r8: %08lx %08lx %08lx %08lx %08lx\n",
         jdb_get(&s->r[8]), jdb_get(&s->r[9]), jdb_get(&s->r[10]), jdb_get(&s->r[11]),
         jdb_get(&s->r[12]));

  printf("usr: sp %08lx lr %08lx\n",
         jdb_get(&s->sp_usr), jdb_get(&s->lr_usr));
  printf("irq: sp %08lx lr %08lx psr %08lx\n",
         jdb_get(&s->irq.sp), jdb_get(&s->irq.lr), jdb_get(&s->irq.spsr));
  printf("fiq: sp %08lx lr %08lx psr %08lx\n",
         jdb_get(&s->fiq.sp), jdb_get(&s->fiq.lr), jdb_get(&s->fiq.spsr));
  printf("r8: %08lx %08lx %08lx %08lx %08lx\n",
         jdb_get(&s->r_fiq[0]), jdb_get(&s->r_fiq[1]), jdb_get(&s->r_fiq[2]),
         jdb_get(&s->r_fiq[3]), jdb_get(&s->r_fiq[4]));

  printf("abt: sp %08lx lr %08lx psr %08lx\n",
         jdb_get(&s->abt.sp), jdb_get(&s->abt.lr), jdb_get(&s->abt.spsr));
  printf("und: sp %08lx lr %08lx psr %08lx\n",
         jdb_get(&s->und.sp), jdb_get(&s->und.lr), jdb_get(&s->und.spsr));
  printf("svc: sp %08lx lr %08lx psr %08lx\n",
         jdb_get(&s->svc.sp), jdb_get(&s->svc.lr), jdb_get(&s->svc.spsr));
}

PUBLIC
void
Vm::show_short(String_buffer *buf)
{
  buf->printf(" utcb:%lx pc:%lx ", (Mword)state_for_dbg, (Mword)jdb_get(&state_for_dbg->pc));
}

IMPLEMENT
void
Vm::Vm_log::print(String_buffer *buf) const
{
  buf->printf("%s: pc:%08lx/%03lx psr:%lx er:%lx r0:%lx r1:%lx",
              is_entry ? "entry" : "exit ",
              pc, pending_events, cpsr, exit_reason, r0, r1);
}

PUBLIC static inline
void
Vm::log_vm(Vm_state *state, bool is_entry)
{
  LOG_TRACE("VM entry/entry", "VM", current(), Vm_log,
      l->is_entry = is_entry;
      l->pc = state->pc;
      l->cpsr = state->cpsr;
      l->exit_reason = state->exit_reason;
      l->pending_events = state->pending_events;
      l->r0 = state->r[0];
      l->r1 = state->r[1];
  );
}

// --------------------------------------------------------------------------
IMPLEMENTATION [!debug && arm_em_tz]:

PUBLIC static inline
void
Vm::log_vm(Vm_state *, bool)
{}
