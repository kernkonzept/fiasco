INTERFACE:

#include "kobject.h"
#include "kobject_helper.h"
#include "thread.h"

class Thread_object : public Thread, public Kobject_helper_base
{
private:
  struct Remote_syscall
  {
    Thread *thread;
    L4_msg_tag result;
    bool have_recv;
  };
};

class Obj_cap : public L4_obj_ref
{
};


// ---------------------------------------------------------------------------
IMPLEMENTATION:

#include "context.h"
#include "fpu.h"
#include "irq_chip.h"
#include "map_util.h"
#include "processor.h"
#include "task.h"
#include "thread_state.h"
#include "timer.h"



PUBLIC inline
Obj_cap::Obj_cap(L4_obj_ref const &o) : L4_obj_ref(o) {}

PUBLIC inline NEEDS["kobject.h"]
Kobject_iface * __attribute__((nonnull))
Obj_cap::deref(L4_fpage::Rights *rights, bool dbg = false)
{
  Thread *current = current_thread();
  if (op() & L4_obj_ref::Ipc_reply)
    {
      *rights = current->caller_rights();
      Thread *ca = static_cast<Thread*>(current->caller());
      if (EXPECT_TRUE(!dbg && ca))
        current->reset_caller();
      return ca;
    }

  if (EXPECT_FALSE(special()))
    {
      if (!self())
        return nullptr;

      *rights = L4_fpage::Rights::CWS();
      return current;
    }

  return current->space()->lookup_local(cap(), rights);
}

PUBLIC explicit
Thread_object::Thread_object(Ram_quota *q) : Thread(q) {}

PUBLIC explicit
Thread_object::Thread_object(Ram_quota *q, Context_mode_kernel k)
: Thread(q, k) {}

PUBLIC virtual
bool
Thread_object::put() override
{ return dec_ref() == 0; }



PUBLIC
void
Thread_object::operator delete(Thread_object *t, std::destroying_delete_t)
{
  Ram_quota * const q = t->_quota;

  LOG_TRACE("Kobject delete", "del", current(), Log_destroy,
      l->id = t->dbg_id();
      l->obj = t;
      l->type = cxx::Typeid<Thread>::get();
      l->ram = q->current());

  t->~Thread_object();
  Kmem_alloc::allocator()->q_free(q, Bytes(Thread::Size), t);
}


PUBLIC
void
Thread_object::destroy(Kobjects_list &reap_list) override
{
  Kobject::destroy(reap_list);

  if (!is_invalid(false))
    check(kill());
  else
    unbind();

  assert(_magic == magic);
}

PUBLIC
void
Thread_object::invoke(L4_obj_ref self, L4_fpage::Rights rights,
                      Syscall_frame *f, Utcb *utcb) override
{
  L4_obj_ref::Operation op = f->ref().op();
  if (((op != 0) && !(op & L4_obj_ref::Ipc_send))
      || (op & L4_obj_ref::Ipc_reply)
      || f->tag().proto() != L4_msg_tag::Label_thread)
    {
      /* we do IPC */
      Thread *ct = current_thread();
      Thread *sender = nullptr;
      Thread *partner = nullptr;
      bool have_rcv = false;

      if (EXPECT_FALSE(!check_sys_ipc(op, &partner, &sender, &have_rcv)))
        {
          f->tag(commit_error(utcb, L4_error::Not_existent));
          return;
        }

      ct->do_ipc(f->tag(), f->from_spec(), partner, have_rcv, sender,
                 f->timeout(), f, rights);
      return;
    }

  if (EXPECT_FALSE(f->tag().words() < 1))
    {
      f->tag(commit_result(-L4_err::EMsgtooshort));
      return;
    }

  Utcb *out = self.have_recv() ? utcb : utcb_dummy();
  L4_msg_tag tag;
  switch (Op{utcb->values[0] & Opcode_mask})
    {
    case Op::Control:
      tag = sys_control(rights, f->tag(), utcb, out);
      break;
    case Op::Ex_regs:
      tag = sys_ex_regs(f->tag(), utcb, out);
      break;
    case Op::Switch:
      tag = sys_thread_switch(f->tag(), utcb, out);
      break;
    case Op::Stats:
      tag = sys_thread_stats(f->tag(), utcb, out);
      break;
    case Op::Vcpu_resume:
      tag = sys_vcpu_resume(f->tag(), utcb, out);
      break;
    case Op::Register_del_irq:
      tag = sys_register_delete_irq(f->tag(), utcb, out);
      break;
    case Op::Modify_senders:
      tag = sys_modify_senders(f->tag(), utcb, out);
      break;
    case Op::Vcpu_control:
      tag = sys_vcpu_control(rights, f->tag(), utcb, out);
      break;
    case Op::Register_doorbell_irq:
      tag = sys_register_doorbell_irq(f->tag(), utcb);
      break;
    default:
      tag = invoke_arch(f->tag(), utcb, out);
      break;
    }

  if (self.have_recv() || tag.has_error())
    f->tag(tag);
}


PRIVATE inline
L4_msg_tag
Thread_object::sys_vcpu_resume(L4_msg_tag const &tag, Utcb const *utcb, Utcb *)
{
  if (this != current() || !(state() & Thread_vcpu_enabled))
    return commit_result(-L4_err::EInval);

  Space *s = space();
  Vcpu_state *vcpu = vcpu_state().access(true);

  L4_obj_ref user_task = vcpu->user_task;
  if (user_task.valid() && user_task.op() == 0)
    {
      L4_fpage::Rights task_rights = L4_fpage::Rights(0);
      Task *task = cxx::dyn_cast<Task*>(s->lookup_local(user_task.cap(),
                                                         &task_rights));

      if (EXPECT_FALSE(task && !(task_rights & L4_fpage::Rights::CS())))
        return commit_result(-L4_err::EPerm);

      if (task != vcpu_user_space())
        vcpu_set_user_space(task);

      reinterpret_cast<Mword &>(vcpu->user_task) |= L4_obj_ref::Ipc_send;
    }
  else if (user_task.op() == L4_obj_ref::Ipc_reply)
    vcpu_set_user_space(nullptr);

  L4_snd_item_iter snd_items(utcb, tag.words());
  int items = tag.items();
  if (vcpu_user_space())
    {
      for (; items && snd_items.next(); --items)
        {
          // Must be destroyed _after_ releasing the existence lock below!
          Reap_list reap_list;

          // in this case we already have a counted reference managed by vcpu_user_space()
          auto guard = switch_lock_guard(static_cast<Task *>(vcpu_user_space())->existence_lock);
          if (!guard.is_valid())
            return commit_result(-L4_err::ENoent);

          cpu_lock.clear();

          L4_snd_item_iter::Item const *const item = snd_items.get();
          L4_fpage sfp(item->d);
          L4_error err = fpage_map(space(), sfp, vcpu_user_space(),
                                   L4_fpage::all_spaces(), item->b,
                                   reap_list.list());

          cpu_lock.lock();

          if (EXPECT_FALSE(!err.ok()))
            return commit_error(utcb, err);
        }
    }

  if ((vcpu->_saved_state & Vcpu_state::F_irqs)
      && (vcpu->sticky_flags & Vcpu_state::Sf_irq_pending))
    {
      assert(cpu_lock.test());
      do_ipc(L4_msg_tag(), 0, nullptr, true, nullptr,
             L4_timeout_pair(L4_timeout::Zero, L4_timeout::Zero),
             &vcpu->_ipc_regs, L4_fpage::Rights::FULL());

      vcpu = vcpu_state().access(true);

      if (EXPECT_TRUE(!vcpu->_ipc_regs.tag().has_error()
                      || this->utcb().access(true)->error.error() == L4_error::R_timeout))
        {
          vcpu->_regs.set_ipc_upcall();

          Address sp;

          // tried to resume to user mode, so an IRQ enters from user mode
          if (vcpu->_saved_state & Vcpu_state::F_user_mode)
            sp = vcpu->_entry_sp;
          else
            sp = vcpu->_regs.s.sp();

          LOG_TRACE("VCPU events", "vcpu", this, Vcpu_log,
              l->type = 4;
              l->state = vcpu->state;
              l->ip = vcpu->_entry_ip;
              l->sp = sp;
              l->space = static_cast<Task*>(_space.vcpu_aware())->dbg_id();
              );

          vcpu_return_to_kernel(vcpu->_entry_ip, sp, vcpu_state().usr().get());
        }
    }

  vcpu->state = vcpu->_saved_state;
  Task *target_space = nonull_static_cast<Task*>(space());
  bool user_mode = false;

  if (vcpu->state & Vcpu_state::F_user_mode)
    {
      if (!vcpu_user_space())
        return commit_result(-L4_err::ENoent);

      user_mode = true;

      if (!(vcpu->state & Vcpu_state::F_fpu_enabled))
        {
          state_add_dirty(Thread_vcpu_fpu_disabled);
          Fpu::fpu.current().disable();
        }
      else
        state_del_dirty(Thread_vcpu_fpu_disabled);

      target_space = static_cast<Task*>(vcpu_user_space());

      arch_load_vcpu_user_state(vcpu);
    }

  LOG_TRACE("VCPU events", "vcpu", this, Vcpu_log,
      l->type = 0;
      l->state = vcpu->state;
      l->ip = vcpu->_regs.s.ip();
      l->sp = vcpu->_regs.s.sp();
      l->space = target_space->dbg_id();
      );

  return commit_result(target_space->resume_vcpu(this, vcpu, user_mode));
}

PRIVATE inline NOEXPORT NEEDS["processor.h"]
L4_msg_tag
Thread_object::sys_modify_senders(L4_msg_tag tag, Utcb const *in, Utcb * /*out*/)
{
  // Prevent vanishing of this thread during the whole operation.
  Ref_ptr<Thread> self(this);

  // Only threads running on our CPU are allowed to iterate our sender list.
  if (current_cpu() != home_cpu())
    return commit_result(-L4_err::EInval);

  if (sender_list()->cursor())
    return commit_result(-L4_err::EBusy);

  if constexpr (0) // Intentionally disabled, only used for diagnostics
    printf("MODIFY ID (%08lx:%08lx->%08lx:%08lx\n",
           in->values[1], in->values[2],
           in->values[3], in->values[4]);


  int elems = tag.words();

  if (elems < 5)
    return Kobject_iface::commit_result(0);

  --elems;

  elems = elems / 4;

  ::Prio_list_elem *c = sender_list()->first();
  while (c)
    {
      // this is kind of arbitrary
      for (int cnt = 50; c && cnt > 0; --cnt)
        {
          Sender *s = Sender::cast(c);
          s->modify_label(&in->values[1], elems);
          c = sender_list()->next(c);
        }

      if (!c)
        break;

      sender_list()->cursor(c);
      Proc::preemption_point();

      // After resuming from preemption we have to enforce again that only
      // threads running on our CPU are allowed to iterate our sender list.
      if (current_cpu() != home_cpu())
        {
          sender_list()->cursor(nullptr);
          return commit_result(-L4_err::EInval);
        }

      c = sender_list()->cursor();
    }

  sender_list()->cursor(nullptr);
  return Kobject_iface::commit_result(0);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Thread_object::sys_register_delete_irq(L4_msg_tag tag, Utcb const *in, Utcb * /*out*/)
{
  L4_snd_item_iter snd_items(in, tag.words());

  if (!tag.items() || !snd_items.next())
    return Kobject_iface::commit_result(-L4_err::EInval);

  L4_fpage bind_irq(snd_items.get()->d);
  if (EXPECT_FALSE(!bind_irq.is_objpage()))
    return Kobject_iface::commit_error(in, L4_error::Overflow);

  Context *const c_thread = ::current();
  Space *const c_space = c_thread->space();
  L4_fpage::Rights irq_rights = L4_fpage::Rights(0);
  Irq_base *irq
    = Irq_base::dcast(c_space->lookup_local(bind_irq.obj_index(), &irq_rights));

  if (!irq)
    return Kobject_iface::commit_result(-L4_err::EInval);

  if (EXPECT_FALSE(!(irq_rights & L4_fpage::Rights::CW())))
    return Kobject_iface::commit_result(-L4_err::EPerm);

  if (register_delete_irq(irq))
    return Kobject_iface::commit_result(0);
  else
    return Kobject_iface::commit_result(-L4_err::EBusy);
}


PRIVATE inline NOEXPORT
L4_msg_tag
Thread_object::sys_control(L4_fpage::Rights rights, L4_msg_tag tag,
                           Utcb const *utcb, Utcb *out)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  if (EXPECT_FALSE(tag.words() < 6))
    return commit_result(-L4_err::EInval);

  Mword flags = utcb->values[0];

  Mword _old_pager = cxx::int_value<Cap_index>(_pager.raw()) << L4_obj_ref::Cap_shift;
  Mword _old_exc_handler = cxx::int_value<Cap_index>(_exc_handler.raw()) << L4_obj_ref::Cap_shift;

  Thread_ptr _new_pager(Thread_ptr::Invalid);
  Thread_ptr _new_exc_handler(Thread_ptr::Invalid);

  if (flags & Ctl_set_pager)
    _new_pager = Thread_ptr(Cap_index(utcb->values[1] >> L4_obj_ref::Cap_shift));

  if (flags & Ctl_set_exc_handler)
    _new_exc_handler = Thread_ptr(Cap_index(utcb->values[2] >> L4_obj_ref::Cap_shift));

  if (flags & Ctl_bind_task)
    {
      L4_fpage::Rights task_rights = L4_fpage::Rights(0);
      Task *task = Ko::deref<Task>(&tag, utcb, &task_rights);
      if (!task)
        return tag;

      if (EXPECT_FALSE(!(task_rights & L4_fpage::Rights::CS())))
        return commit_result(-L4_err::EPerm);

      if (EXPECT_FALSE(!(task->caps() & Task::Caps::threads())))
        return commit_result(-L4_err::EInval);

      User_ptr<Utcb> utcb_addr =
        User_ptr<Utcb>(reinterpret_cast<Utcb*>(utcb->values[5]));

      if (EXPECT_FALSE(!bind(task, utcb_addr)))
        return commit_result(-L4_err::EInval); // unbind first !!
    }

  Mword del_state = 0;
  Mword add_state = 0;

  long res = control(_new_pager, _new_exc_handler);

  if (res < 0)
    return commit_result(res);

  if (flags & Ctl_alien_thread)
    {
      if (utcb->values[4] & Ctl_alien_thread)
        {
          add_state |= Thread_alien;
          del_state |= Thread_dis_alien;
        }
      else
        del_state |= Thread_alien;
    }

  out->values[1] = _old_pager;
  out->values[2] = _old_exc_handler;

  if (del_state || add_state)
    {
      assert(!(add_state & Thread_ready_mask));
      xcpu_state_change(~del_state, add_state, true);
    }

  return commit_result(0, 3);
}


PRIVATE inline NOEXPORT
L4_msg_tag
Thread_object::sys_vcpu_control(L4_fpage::Rights, L4_msg_tag const &tag,
                                Utcb const *utcb, Utcb * /* out */)
{
  User_ptr<Vcpu_state> vcpu(nullptr);

  if (tag.words() >= 2)
    vcpu = User_ptr<Vcpu_state>(reinterpret_cast<Vcpu_state*>(utcb->values[1]));

  Mword del_state = 0;
  Mword add_state = 0;

  if (vcpu)
    {
      Mword size = sizeof(Vcpu_state);
      if (utcb->values[0] & Vcpu_ctl_extended_vcpu)
        {
          if (!arch_ext_vcpu_enabled())
            return commit_result(-L4_err::ENosys);
          size = Config::ext_vcpu_size();
          add_state |= Thread_ext_vcpu_enabled;
        }

      Space::Ku_mem const *vcpu_m = space()->find_ku_mem(vcpu, size);
      if (!vcpu_m)
        return commit_result(-L4_err::EInval);

      Mword ret = arch_check_vcpu_state(add_state & Thread_ext_vcpu_enabled);
      if (ret != 0)
        return commit_result(ret);

      add_state |= Thread_vcpu_enabled;
      _vcpu_state.set(vcpu, vcpu_m->kern_addr(vcpu));

      Vcpu_state *s = _vcpu_state.access();
      arch_init_vcpu_state(s, add_state & Thread_ext_vcpu_enabled);
      arch_update_vcpu_state(s);
    }
  else
    return commit_result(-L4_err::EInval);

  /* hm, we do not allow to disable vCPU mode, it's one way enable
  else
    {
      del_state |= Thread_vcpu_enabled | Thread_vcpu_user_mode
                   | Thread_vcpu_fpu_disabled | Thread_ext_vcpu_enabled;
    }
  */

  assert(!(add_state & Thread_ready_mask));
  xcpu_state_change(~del_state, add_state, true);

  return commit_result(0);
}


// -------------------------------------------------------------------
// Thread::ex_regs class system calls

PUBLIC
bool
Thread_object::ex_regs(Address ip, Address sp,
                Address *o_ip = nullptr, Address *o_sp = nullptr,
                Mword *o_flags = nullptr, Mword ops = 0)
{
  if (current() == this)
    spill_user_state();

  if (o_sp)
    *o_sp = user_sp();
  if (o_ip)
    *o_ip = user_ip();
  if (o_flags)
    *o_flags = user_flags();

  // Changing the run state is only possible when the thread is not in
  // an exception.
  if (!(ops & Exr_cancel) && (state() & Thread_in_exception))
    // XXX Maybe we should return false here.  Previously, we actually
    // did so, but we also actually didn't do any state modification.
    // If you change this value, make sure the logic in
    // sys_thread_ex_regs still works (in particular,
    // ex_regs_cap_handler and friends should still be called).
    return true;

  if (!ex_regs_arch(ops))
    return false;

  if (state() & Thread_dead)	// resurrect thread
    state_change_dirty(~Thread_dead, Thread_ready);

  else if (ops & Exr_cancel)
    // cancel ongoing IPC or other activity
    state_add_dirty(Thread_cancel | Thread_ready);

  if (ops & Exr_trigger_exception)
    {
      extern char leave_by_trigger_exception[];
      do_trigger_exception(regs(), leave_by_trigger_exception);
    }

  if (ip != ~0UL)
    user_ip(ip);

  if (sp != ~0UL)
    user_sp (sp);

  if (current() == this)
    fill_user_state();

  return true;
}

PRIVATE inline NOEXPORT
L4_msg_tag
Thread_object::ex_regs(Utcb const *utcb, Utcb *out)
{
  Address ip = utcb->values[1];
  Address sp = utcb->values[2];
  Mword flags;
  Mword ops = utcb->values[0];

  LOG_TRACE("Ex-regs", "exr", current(), Log_thread_exregs,
      l->id = dbg_id();
      l->ip = ip; l->sp = sp; l->op = ops;);

  if (!ex_regs(ip, sp, &ip, &sp, &flags, ops))
    return commit_result(-L4_err::EInval);

  out->values[0] = flags;
  out->values[1] = ip;
  out->values[2] = sp;

  return commit_result(0, 3);
}

PRIVATE static
Context::Drq::Result
Thread_object::handle_remote_ex_regs(Drq *, Context *self, void *p)
{
  Remote_syscall *params = reinterpret_cast<Remote_syscall*>(p);
  params->result = nonull_static_cast<Thread_object *>(self)->ex_regs(
    params->thread->utcb().access(),
    params->have_recv ? params->thread->utcb().access() : utcb_dummy());
  return params->result.proto() == 0 ? Drq::need_resched() : Drq::done();
}

PRIVATE inline NOEXPORT
L4_msg_tag
Thread_object::sys_ex_regs(L4_msg_tag const &tag, Utcb *utcb, Utcb *out)
{
  if (tag.words() != 3)
    return commit_result(-L4_err::EInval);

  if (current() == this)
    return ex_regs(utcb, out);

  Remote_syscall params;
  params.thread = current_thread();
  params.have_recv = (utcb == out);

  drq(handle_remote_ex_regs, &params);
  return params.result;
}

PRIVATE inline NOEXPORT NEEDS["timer.h"]
L4_msg_tag
Thread_object::sys_thread_switch(L4_msg_tag const & /*tag*/, Utcb const * /*utcb*/,
                                 Utcb *out)
{
  Context *curr = current();

  if (curr == this)
    return commit_result(0);

  if (current_cpu() != home_cpu())
    return commit_result(0);

#ifdef FIXME
  Sched_context * const cs = current_sched();
#endif

  if (curr != this && (state() & Thread_ready_mask))
    {
      curr->schedule_if(curr->switch_exec_locked(this, Not_Helping) != Switch::Ok);
      reinterpret_cast<Utcb::Time_val*>(out->values)->t = 0; // Assume timeslice was used up
      return commit_result(0, Utcb::Time_val::Words);
    }

#if 0 // FIXME: provide API for multiple sched contexts
      // Compute remaining quantum length of timeslice
      regs->left(timeslice_timeout.cpu(cpu())->get_timeout(Timer::system_clock()));

      // Yield current global timeslice
      cs->owner()->switch_sched(cs->id() ? cs->next() : cs);
#endif
  reinterpret_cast<Utcb::Time_val*>(out->values)->t
    = timeslice_timeout.current()->get_timeout(Timer::system_clock());
  curr->schedule();

  return commit_result(0, Utcb::Time_val::Words);
}



// -------------------------------------------------------------------
// Gather statistics information about thread execution

PRIVATE inline NOEXPORT
Context::Drq::Result
Thread_object::sys_thread_stats_remote(void *data)
{
  // For threads whose home CPU is the invalid CPU, this method is executed in
  // the context of another thread, don't consume CPU time in that case.
  if (this == current())
    update_consumed_time();
  *static_cast<Clock::Time *>(data) = consumed_time();
  return Drq::done();
}

PRIVATE static
Context::Drq::Result FIASCO_FLATTEN
Thread_object::handle_sys_thread_stats_remote(Drq *, Context *self, void *data)
{
  return nonull_static_cast<Thread_object*>(self)->sys_thread_stats_remote(data);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Thread_object::sys_thread_stats(L4_msg_tag const &/*tag*/, Utcb const * /*utcb*/, Utcb *out)
{
  Clock::Time value;

  if (home_cpu() != current_cpu())
    drq(handle_sys_thread_stats_remote, &value);
  else
    {
      // Respect the fact that the consumed time is only updated on context switch
      if (this == current())
        update_consumed_time();
      value = consumed_time();
    }

  reinterpret_cast<Utcb::Time_val *>(out->values)->t = value;

  return commit_result(0, Utcb::Time_val::Words);
}

namespace {

static Kobject_iface * FIASCO_FLATTEN
thread_factory(Ram_quota *q, Space *,
               L4_msg_tag, Utcb const *, Utcb *,
               int *err, unsigned *)
{
  *err = L4_err::ENomem;
  return new (q) Thread_object(q);
}

static inline
void __attribute__((constructor)) FIASCO_INIT_SFX(thread_register_factory)
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_thread, thread_factory);
}

}

//-----------------------------------------------------------------------------
IMPLEMENTATION [!irq_direct_inject]:

PRIVATE inline
L4_msg_tag
Thread_object::sys_register_doorbell_irq(L4_msg_tag, Utcb const *)
{
  return commit_result(-L4_err::ENosys);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [irq_direct_inject]:

PRIVATE
L4_msg_tag
Thread_object::sys_register_doorbell_irq(L4_msg_tag tag, Utcb const *in)
{
  L4_snd_item_iter snd_items(in, tag.words());

  if (!tag.items() || !snd_items.next())
    return Kobject_iface::commit_result(-L4_err::EInval);

  L4_fpage bind_irq(snd_items.get()->d);
  if (EXPECT_FALSE(!bind_irq.is_objpage()))
    return Kobject_iface::commit_error(in, L4_error::Overflow);

  Context *const c_thread = ::current();
  Space *const c_space = c_thread->space();
  L4_fpage::Rights irq_rights = L4_fpage::Rights(0);
  Irq_base *irq
    = Irq_base::dcast(c_space->lookup_local(bind_irq.obj_index(), &irq_rights));

  if (!irq)
    return Kobject_iface::commit_result(-L4_err::EInval);

  if (EXPECT_FALSE(!(irq_rights & L4_fpage::Rights::CW())))
    return Kobject_iface::commit_result(-L4_err::EPerm);

  if (register_doorbell_irq(irq))
    return Kobject_iface::commit_result(0);
  else
    return Kobject_iface::commit_result(-L4_err::EBusy);
}
