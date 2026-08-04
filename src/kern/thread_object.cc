#include "thread_object.h"

#include "context.h"
#include "fpu.h"
#include "irq_chip.h"
#include "map_util.h"
#include "processor.h"
#include "task.h"
#include "thread_state.h"
#include "kobject_rpc.h"
#include <system_clock.h>

#include <thread_vcpu.h>
#include <entry.h>
#include <globalconfig.h>

#ifdef CONFIG_JDB
#include <exregs_log.h>
#endif

inline
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
      L4_msg_tag res;
      Task *task = s->lookup_local(user_task.cap(), L4_fpage::Rights::CS())
        .deref<Task>(&res);
      if (EXPECT_FALSE(!task))
        return res;

      if (task != vcpu_user_space())
        set_vcpu_user_space(task);

      reinterpret_cast<Mword &>(vcpu->user_task) |= L4_obj_ref::Ipc_send;
    }
  else if (user_task.op() == L4_obj_ref::Ipc_reply)
    set_vcpu_user_space(nullptr);

  L4_snd_item_iter snd_items(utcb, tag.words());
  int items = tag.items();
  if (vcpu_user_space())
    for (; items && snd_items.more(); --items)
      {
        if (EXPECT_FALSE(!snd_items.next()))
          break;

        Reap_list rl;
        L4_error err;

          {
            // in this case we already have a counted reference managed by vcpu_user_space()
            Lock_guard<Lock> guard;
            if (!guard.check_and_lock(&static_cast<Task *>(vcpu_user_space())->existence_lock))
              return commit_result(-L4_err::ENoent);

            cpu_lock.clear();

            L4_snd_item_iter::Item const *const item = snd_items.get();
            L4_fpage sfp(item->d);

            err = fpage_map(space(), sfp,
                            vcpu_user_space(), L4_fpage::all_spaces(),
                            item->b, &rl);
          }

        rl.del();

        cpu_lock.lock();

        if (EXPECT_FALSE(!err.ok()))
          return commit_error(utcb, err);
      }

  if (vcpu->saved_irqs_enabled() && vcpu->pending_irqs())
    {
      assert(cpu_lock.test());
      do_ipc_open_wait(L4_msg_tag(),
                       L4_timeout_pair(L4_timeout::Zero, L4_timeout::Zero),
                       &vcpu->_ipc_regs);

      vcpu = vcpu_state().access(true);

      if (EXPECT_TRUE(!vcpu->_ipc_regs.tag().has_error()
                      || this->utcb().access(true)->error.error() == L4_error::R_timeout))
        {
          vcpu->_regs.set_ipc_upcall();

          Address sp;

          // tried to resume to user mode, so an IRQ enters from user mode
          if (vcpu->saved_user_mode())
            sp = vcpu->_entry_sp;
          else
            sp = vcpu->_regs.s.sp();

          LOG_TRACE("VCPU events", "vcpu", this, Vcpu_log,
              l->type = 4;
              l->state = vcpu->state();
              l->ip = vcpu->_entry_ip;
              l->sp = sp;
              l->space = static_cast<Task*>(_cpu_state.space.vcpu_aware())->dbg_id();
              );

          Entry::vcpu_return_to_kernel(this, vcpu->_entry_ip, sp, vcpu_state().usr().get());
        }
    }

  Unsigned16 vstate = vcpu->restore_state();
  Task *target_space = nonull_static_cast<Task*>(space());
  bool user_mode = false;

  if (vstate & Vcpu_state::F_user_mode)
    {
      if (!vcpu_user_space())
        return commit_result(-L4_err::ENoent);

      user_mode = true;

      if (!(vstate & Vcpu_state::F_fpu_enabled))
        {
          state.add_dirty(Thread_vcpu_fpu_disabled);
          Fpu::fpu.current().disable();
        }
      else
        state.del_dirty(Thread_vcpu_fpu_disabled);

      target_space = static_cast<Task*>(vcpu_user_space());

      arch_load_vcpu_user_state(vcpu, true);
    }

  LOG_TRACE("VCPU events", "vcpu", this, Vcpu_log,
      l->type = 0;
      l->state = vstate;
      l->ip = vcpu->_regs.s.ip();
      l->sp = vcpu->_regs.s.sp();
      l->space = target_space->dbg_id();
      );

  return commit_result(target_space->resume_vcpu(this, vcpu, user_mode));
}

inline
L4_msg_tag
Thread_object::sys_modify_senders(L4_msg_tag tag, Utcb const *in, Utcb * /*out*/)
{
  // Only threads running on our CPU are allowed to iterate our sender list.
  if (current() != this)
    return commit_result(-L4_err::EInval);

  if (sender_list()->cursor())
    return Kobject_iface::commit_result(-L4_err::EBusy);

  if (0)
    printf("MODIFY ID (%08lx:%08lx->%08lx:%08lx\n",
           in->values[1], in->values[2],
           in->values[3], in->values[4]);


  int elems = tag.words();

  if (elems < 5)
    return Kobject_iface::commit_result(0);

  Mword flags = in->values[0];
  if (flags & 0x10000)
    this->rcu_wait();

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

inline
L4_msg_tag
Thread_object::sys_register_delete_irq(L4_msg_tag tag, Utcb const *in, Utcb * /*out*/)
{
  Kobject_iface *i = Ko::first_cap(&tag, in, L4_fpage::Rights::CW()).deref(&tag);
  if (!i)
    return tag;

  Irq_base *irq = Irq_base::dcast(i);

  if (!irq)
    return Kobject_iface::commit_result(-L4_err::EInval);

  if (register_delete_irq(irq))
    return Kobject_iface::commit_result(0);
  else
    return Kobject_iface::commit_result(-L4_err::EBusy);
}


inline
L4_msg_tag
Thread_object::sys_control(L4_fpage::Rights rights, L4_msg_tag tag,
                           Utcb const *utcb, Utcb *out)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  if (EXPECT_FALSE(tag.words() < 6))
    return commit_result(-L4_err::EInval);

  Task *task = nullptr;
  User_ptr<Utcb> utcb_addr(0);

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
      task = Ko::first_cap(&tag, utcb, L4_fpage::Rights::CS()).deref<Task>(&tag);
      if (!task)
        return tag;

      if (EXPECT_FALSE(!(task->caps() & Task::Caps::threads())))
        return commit_result(-L4_err::EInval);

      utcb_addr = User_ptr<Utcb>(reinterpret_cast<Utcb*>(utcb->values[5]));

      if (EXPECT_FALSE(!bind(task, utcb_addr)))
        return commit_result(-L4_err::EInval); // unbind first !!
    }

  Mword del_state = 0;
  Mword add_state = 0;

  long res = control(_new_pager, _new_exc_handler);

  if (res < 0)
    return commit_result(res);

  if ((res = sys_control_arch(utcb, out)) < 0)
    return commit_result(res);

  out->values[1] = _old_pager;
  out->values[2] = _old_exc_handler;

  if (del_state || add_state)
    {
      state.change(~del_state, add_state);
      if ((add_state & Thread_ready) && xcpu_lazy_ready_enqueue())
        current()->switch_to_locked(this);
    }

  return commit_result(0, 3);
}


inline
L4_msg_tag
Thread_object::sys_vcpu_control(L4_fpage::Rights, L4_msg_tag const &tag,
                                Utcb const *utcb, Utcb * /* out */)
{
  User_ptr<Vcpu_state> vcpu(0);

  if (tag.words() >= 2)
    vcpu = User_ptr<Vcpu_state>(reinterpret_cast<Vcpu_state*>(utcb->values[1]));

  Mword del_state = 0;
  Thread_state add_state = 0;

  if (vcpu)
    {
      Mword size = sizeof(Vcpu_state);
      bool ext_state = false;
      if (access_once(&utcb->values[0]) & Vcpu_ctl_extended_vcpu)
        {
          if (!Thread_vcpu::ext_vcpu_available())
            return commit_result(-L4_err::ENosys);
          size = Config::ext_vcpu_size();
          add_state.vcpu_enabled() = true;
          ext_state = true;
        }

      Space::Ku_mem const *vcpu_m = space()->find_ku_mem(vcpu, size);
      if (!vcpu_m)
        return commit_result(-L4_err::EInval);

      if (int ret = Thread_vcpu::pre_check(this, add_state.vcpu_enabled()))
        return commit_result(ret);

      add_state.vcpu_enabled() = true;
      _vcpu_state.set(vcpu, vcpu_m->kern_addr(vcpu));

      Vcpu_state *s = new (_vcpu_state.access()) Vcpu_state;
      Thread_vcpu::init_state(this, s, ext_state);
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

  state.change(~del_state, add_state.state);
  if (add_state.ready() && xcpu_lazy_ready_enqueue())
    current()->switch_to_locked(this);

  return commit_result(0);
}


// -------------------------------------------------------------------
// Thread::ex_regs class system calls

bool
Thread_object::ex_regs(Address ip, Address sp,
                Address *o_ip, Address *o_sp, Mword *o_flags,
                Mword ops)
{
  if (current() == this)
    spill_user_state();

  if (o_sp) *o_sp = user_sp();
  if (o_ip) *o_ip = user_ip();
  if (o_flags) *o_flags = regs()->flags();

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
    state.change_dirty(~Thread_dead, Thread_ready);

  else if (ops & Exr_cancel)
    // cancel ongoing IPC or other activity
    state.add_dirty(Thread_cancel | Thread_ready);

  if (ops & Exr_trigger_exception)
    {
      extern char leave_by_trigger_exception[];
      do_trigger_exception(regs(), leave_by_trigger_exception);
    }

  if (ip != ~0UL)
    user_ip(ip);

  if (sp != ~0UL)
    user_sp(sp);

  if (current() == this)
    fill_user_state();

  return true;
}

inline
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

Context::Drq::Result
Thread_object::handle_remote_ex_regs(Drq *, Context *self, void *p)
{
  Remote_syscall *params = reinterpret_cast<Remote_syscall*>(p);
  params->result = nonull_static_cast<Thread_object *>(self)->ex_regs(
    params->thread->utcb().access(),
    params->have_recv ? params->thread->utcb().access() : utcb_dummy());
  return params->result.proto() == 0 ? Drq::need_resched() : Drq::done();
}

inline
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

inline
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
      regs->left(timeslice_timeout.cpu(cpu())->get_timeout(System_clock::clock()));

      // Yield current global timeslice
      cs->owner()->switch_sched(cs->id() ? cs->next() : cs);
#endif
  reinterpret_cast<Utcb::Time_val*>(out->values)->t
    = timeslice_timeout.current()->get_timeout(System_clock::clock());
  curr->schedule();

  return commit_result(0, Utcb::Time_val::Words);
}



// -------------------------------------------------------------------
// Gather statistics information about thread execution

inline
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

Context::Drq::Result FIASCO_FLATTEN
Thread_object::handle_sys_thread_stats_remote(Drq *, Context *self, void *data)
{
  return nonull_static_cast<Thread_object*>(self)->sys_thread_stats_remote(data);
}

inline
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

void
Thread_object::operator delete(void *_t) noexcept
{
  Thread_object * const t = cxx::launder(nonull_static_cast<Thread_object*>(_t));
  Ram_quota * const q = t->_quota;
  Kmem_alloc::allocator()->q_free(q, Bytes(Thread::Size), t);

  LOG_TRACE("Kobject delete", "del", current(), Log_destroy,
      l->id = t->dbg_id();
      l->obj = t;
      l->type = cxx::Typeid<Thread>::get();
      l->ram = q->current());
}

void
Thread_object::destroy(Kobject ***rl)
{
  Kobject::destroy(rl);
  if (!is_invalid(true))
    check(kill());
  else
    unbind();

  assert(_magic == magic);
}

FIASCO_FLATTEN
void
Thread_object::invoke(L4_obj_ref self, L4_fpage::Rights rights,
                      Syscall_frame *f, Utcb *utcb)
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

      if (EXPECT_FALSE(!check_sys_ipc(ct, op, &partner, &sender, &have_rcv)))
        {
          f->tag(commit_error(utcb, L4_error::Not_existent));
          return;
        }

      ct->set_ipc_from_spec(f->from_spec(), rights, partner);
      ct->do_ipc(f->tag(), partner, Ipc_flags(have_rcv, !sender),
                 sender, f->timeout(), f);
      return;
    }

  if (EXPECT_FALSE(f->tag().words() < 1))
    {
      f->tag(commit_result(-L4_err::EMsgtooshort));
      return;
    }

  Utcb *out = self.have_recv() ? utcb : utcb_dummy();
  L4_msg_tag tag;
  switch (utcb->values[0] & Opcode_mask)
    {
    case Op_control:
      tag = sys_control(rights, f->tag(), utcb, out);
      break;
    case Op_ex_regs:
      tag = sys_ex_regs(f->tag(), utcb, out);
      break;
    case Op_switch:
      tag = sys_thread_switch(f->tag(), utcb, out);
      break;
    case Op_stats:
      tag = sys_thread_stats(f->tag(), utcb, out);
      break;
    case Op_vcpu_resume:
      tag = sys_vcpu_resume(f->tag(), utcb, out);
      break;
    case Op_register_del_irq:
      tag = sys_register_delete_irq(f->tag(), utcb, out);
      break;
    case Op_modify_senders:
      tag = sys_modify_senders(f->tag(), utcb, out);
      break;
    case Op_vcpu_control:
      tag = sys_vcpu_control(rights, f->tag(), utcb, out);
      break;
    default:
      tag = invoke_arch(f->tag(), utcb, out);
      break;
    }

  if (self.have_recv() || tag.has_error())
    f->tag(tag);
}

namespace {

static Kobject_iface * FIASCO_FLATTEN
thread_factory(Ram_quota *q, Space *,
               L4_msg_tag, Utcb const *,
               int *err)
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
