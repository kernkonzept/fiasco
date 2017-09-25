INTERFACE:

#include "kobject.h"
#include "thread.h"

class Thread_object : public Thread
{
private:
  struct Remote_syscall
  {
    Thread *thread;
    L4_msg_tag result;
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
Kobject_iface *
Obj_cap::deref(L4_fpage::Rights *rights = 0, bool dbg = false)
{
  Thread *current = current_thread();
  if (op() & L4_obj_ref::Ipc_reply)
    {
      if (rights) *rights = current->caller_rights();
      Thread *ca = static_cast<Thread*>(current->caller());
      if (EXPECT_TRUE(!dbg && ca))
        current->reset_caller();
      return ca;
    }

  if (EXPECT_FALSE(special()))
    {
      if (!self())
	return 0;

      if (rights) *rights = L4_fpage::Rights::RWX();
      return current_thread();
    }

  return current->space()->lookup_local(cap(), rights);
}

PUBLIC inline NEEDS["kobject.h"]
bool
Obj_cap::revalidate(Kobject_iface *o)
{
  return deref() == o;
}

PUBLIC
Thread_object::Thread_object() : Thread() {}

PUBLIC
Thread_object::Thread_object(Context_mode_kernel k) : Thread(k) {}

PUBLIC virtual
bool
Thread_object::put()
{ return dec_ref() == 0; }



PUBLIC
void
Thread_object::operator delete(void *_t)
{
  Thread_object * const t = nonull_static_cast<Thread_object*>(_t);
  Ram_quota * const q = t->_quota;
  Kmem_alloc::allocator()->q_unaligned_free(q, Thread::Size, t);

  LOG_TRACE("Kobject delete", "del", current(), Log_destroy,
      l->id = t->dbg_id();
      l->obj = t;
      l->type = cxx::Typeid<Thread>::get();
      l->ram = q->current());
}


PUBLIC
void
Thread_object::destroy(Kobject ***rl)
{
  Kobject::destroy(rl);
  if (!is_invalid(false))
    check(kill());
  assert(_magic == magic);
}

PUBLIC
void
Thread_object::invoke(L4_obj_ref /*self*/, L4_fpage::Rights rights, Syscall_frame *f, Utcb *utcb)
{
  L4_obj_ref::Operation op = f->ref().op();
  if (((op != 0) && !(op & L4_obj_ref::Ipc_send))
      || (op & L4_obj_ref::Ipc_reply)
      || f->tag().proto() != L4_msg_tag::Label_thread)
    {
      /* we do IPC */
      Thread *ct = current_thread();
      Thread *sender = 0;
      Thread *partner = 0;
      bool have_rcv = false;

      if (EXPECT_FALSE(!check_sys_ipc(op, &partner, &sender, &have_rcv)))
	{
	  utcb->error = L4_error::Not_existent;
	  return;
	}

      ct->do_ipc(f->tag(), partner, partner, have_rcv, sender,
                 f->timeout(), f, rights);
      return;
    }

  switch (utcb->values[0] & Opcode_mask)
    {
    case Op_control:
      f->tag(sys_control(rights, f->tag(), utcb));
      return;
    case Op_ex_regs:
      f->tag(sys_ex_regs(f->tag(), utcb));
      return;
    case Op_switch:
      f->tag(sys_thread_switch(f->tag(), utcb));
      return;
    case Op_stats:
      f->tag(sys_thread_stats(f->tag(), utcb));
      return;
    case Op_vcpu_resume:
      f->tag(sys_vcpu_resume(f->tag(), utcb));
      return;
    case Op_register_del_irq:
      f->tag(sys_register_delete_irq(f->tag(), utcb, utcb));
      return;
    case Op_modify_senders:
      f->tag(sys_modify_senders(f->tag(), utcb, utcb));
      return;
    case Op_vcpu_control:
      f->tag(sys_vcpu_control(rights, f->tag(), utcb));
      return;
    default:
      f->tag(invoke_arch(f->tag(), utcb));
      return;
    }
}


PRIVATE inline
L4_msg_tag
Thread_object::sys_vcpu_resume(L4_msg_tag const &tag, Utcb *utcb)
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

      if (EXPECT_FALSE(task && !(task_rights & L4_fpage::Rights::W())))
        return commit_result(-L4_err::EPerm);

      if (task != vcpu_user_space())
        vcpu_set_user_space(task);

      reinterpret_cast<Mword &>(vcpu->user_task) |= L4_obj_ref::Ipc_send;
    }
  else if (user_task.op() == L4_obj_ref::Ipc_reply)
    vcpu_set_user_space(0);

  L4_snd_item_iter snd_items(utcb, tag.words());
  int items = tag.items();
  if (vcpu_user_space())
    for (; items && snd_items.more(); --items)
      {
        if (EXPECT_FALSE(!snd_items.next()))
          break;

        // in this case we already have a counted reference managed by vcpu_user_space()
        Lock_guard<Lock> guard;
        if (!guard.check_and_lock(&static_cast<Task *>(vcpu_user_space())->existence_lock))
          return commit_result(-L4_err::ENoent);

        cpu_lock.clear();

        L4_snd_item_iter::Item const *const item = snd_items.get();
        L4_fpage sfp(item->d);

        Reap_list rl;
        L4_error err = fpage_map(space(), sfp,
                                 vcpu_user_space(), L4_fpage::all_spaces(),
                                 item->b, &rl);
        rl.del();

        cpu_lock.lock();

        if (EXPECT_FALSE(!err.ok()))
          return commit_error(utcb, err);
      }

  if ((vcpu->_saved_state & Vcpu_state::F_irqs)
      && (vcpu->sticky_flags & Vcpu_state::Sf_irq_pending))
    {
      assert(cpu_lock.test());
      do_ipc(L4_msg_tag(), 0, 0, true, 0,
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

	  fast_return_to_user(vcpu->_entry_ip, sp, vcpu_state().usr().get());
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

      arch_load_vcpu_user_state(vcpu, true);
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
  if (sender_list()->cursor())
    return Kobject_iface::commit_result(-L4_err::EBusy);

  if (0)
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
	return Kobject_iface::commit_result(0);

      sender_list()->cursor(c);
      Proc::preemption_point();
      c = sender_list()->cursor();
    }
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

  if (EXPECT_FALSE(!(irq_rights & L4_fpage::Rights::X())))
    return Kobject_iface::commit_result(-L4_err::EPerm);

  register_delete_irq(irq);
  return Kobject_iface::commit_result(0);
}


PRIVATE inline NOEXPORT
L4_msg_tag
Thread_object::sys_control(L4_fpage::Rights rights, L4_msg_tag tag, Utcb *utcb)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  if (EXPECT_FALSE(tag.words() < 6))
    return commit_result(-L4_err::EInval);

  Task *task = 0;
  User<Utcb>::Ptr utcb_addr(0);

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
      task = Ko::deref<Task>(&tag, utcb, &task_rights);
      if (!task)
        return tag;

      if (EXPECT_FALSE(!(task_rights & L4_fpage::Rights::CS())))
        return commit_result(-L4_err::EPerm);

      if (EXPECT_FALSE(!(task->caps() & Task::Caps::threads())))
        return commit_result(-L4_err::EInval);

      utcb_addr = User<Utcb>::Ptr((Utcb*)utcb->values[5]);

      if (EXPECT_FALSE(!bind(task, utcb_addr)))
        return commit_result(-L4_err::EInval); // unbind first !!
    }

  Mword del_state = 0;
  Mword add_state = 0;

  long res = control(_new_pager, _new_exc_handler);

  if (res < 0)
    return commit_result(res);

  if ((res = sys_control_arch(utcb)) < 0)
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

  utcb->values[1] = _old_pager;
  utcb->values[2] = _old_exc_handler;

  if (del_state || add_state)
    if (xcpu_state_change(~del_state, add_state, true))
      current()->switch_to_locked(this);

  return commit_result(0, 3);
}


PRIVATE inline NOEXPORT
L4_msg_tag
Thread_object::sys_vcpu_control(L4_fpage::Rights, L4_msg_tag const &tag,
                                Utcb *utcb)
{
  if (!space())
    return commit_result(-L4_err::EInval);

  User<Vcpu_state>::Ptr vcpu(0);

  if (tag.words() >= 2)
    vcpu = User<Vcpu_state>::Ptr((Vcpu_state*)utcb->values[1]);

  Mword del_state = 0;
  Mword add_state = 0;

  if (vcpu)
    {
      Mword size = sizeof(Vcpu_state);
      if (utcb->values[0] & Vcpu_ctl_extended_vcpu)
        {
          if (!arch_ext_vcpu_enabled())
            return commit_result(-L4_err::ENosys);
          size = Config::PAGE_SIZE;
          add_state |= Thread_ext_vcpu_enabled;
        }

      Space::Ku_mem const *vcpu_m
        = space()->find_ku_mem(vcpu, size);

      if (!vcpu_m)
        return commit_result(-L4_err::EInval);

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

  if (xcpu_state_change(~del_state, add_state, true))
    current()->switch_to_locked(this);

  return commit_result(0);
}


// -------------------------------------------------------------------
// Thread::ex_regs class system calls

PUBLIC
bool
Thread_object::ex_regs(Address ip, Address sp,
                Address *o_ip = 0, Address *o_sp = 0, Mword *o_flags = 0,
                Mword ops = 0)
{
  if (!space())
    return false;

  if (current() == this)
    spill_user_state();

  if (o_sp) *o_sp = user_sp();
  if (o_ip) *o_ip = user_ip();
  if (o_flags) *o_flags = user_flags();

  // Changing the run state is only possible when the thread is not in
  // an exception.
  if (!(ops & Exr_cancel) && (state() & Thread_in_exception))
    // XXX Maybe we should return false here.  Previously, we actually
    // did so, but we also actually didn't do any state modification.
    // If you change this value, make sure the logic in
    // sys_thread_ex_regs still works (in particular,
    // ex_regs_cap_handler and friends should still be called).
    return true;

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
Thread_object::ex_regs(Utcb *utcb)
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

  utcb->values[0] = flags;
  utcb->values[1] = ip;
  utcb->values[2] = sp;

  return commit_result(0, 3);
}

PRIVATE static
Context::Drq::Result
Thread_object::handle_remote_ex_regs(Drq *, Context *self, void *p)
{
  Remote_syscall *params = reinterpret_cast<Remote_syscall*>(p);
  params->result = nonull_static_cast<Thread_object*>(self)->ex_regs(params->thread->utcb().access());
  return params->result.proto() == 0 ? Drq::need_resched() : Drq::done();
}

PRIVATE inline NOEXPORT
L4_msg_tag
Thread_object::sys_ex_regs(L4_msg_tag const &tag, Utcb *utcb)
{
  if (tag.words() != 3)
    return commit_result(-L4_err::EInval);

  if (current() == this)
    return ex_regs(utcb);

  Remote_syscall params;
  params.thread = current_thread();

  drq(handle_remote_ex_regs, &params, Drq::Any_ctxt);
  return params.result;
}

PRIVATE inline NOEXPORT NEEDS["timer.h"]
L4_msg_tag
Thread_object::sys_thread_switch(L4_msg_tag const &/*tag*/, Utcb *utcb)
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
      reinterpret_cast<Utcb::Time_val*>(utcb->values)->t = 0; // Assume timeslice was used up
      return commit_result(0, Utcb::Time_val::Words);
    }

#if 0 // FIXME: provide API for multiple sched contexts
      // Compute remaining quantum length of timeslice
      regs->left(timeslice_timeout.cpu(cpu())->get_timeout(Timer::system_clock()));

      // Yield current global timeslice
      cs->owner()->switch_sched(cs->id() ? cs->next() : cs);
#endif
  reinterpret_cast<Utcb::Time_val*>(utcb->values)->t
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
  update_consumed_time();
  *(Clock::Time *)data = consumed_time();
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
Thread_object::sys_thread_stats(L4_msg_tag const &/*tag*/, Utcb *utcb)
{
  Clock::Time value;

  if (home_cpu() != current_cpu())
    drq(handle_sys_thread_stats_remote, &value, Drq::Any_ctxt);
  else
    {
      // Respect the fact that the consumed time is only updated on context switch
      if (this == current())
        update_consumed_time();
      value = consumed_time();
    }

  reinterpret_cast<Utcb::Time_val *>(utcb->values)->t = value;

  return commit_result(0, Utcb::Time_val::Words);
}

namespace {
static Kobject_iface * FIASCO_FLATTEN
thread_factory(Ram_quota *q, Space *,
               L4_msg_tag, Utcb const *,
               int *err)
{
  *err = L4_err::ENomem;
  return new (q) Thread_object();
}

static inline void __attribute__((constructor)) FIASCO_INIT
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_thread, thread_factory);
}
}
