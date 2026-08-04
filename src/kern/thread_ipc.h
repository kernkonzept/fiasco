#pragma once

#include <l4_error.h>
#include <receiver.h>
#include <sender.h>
#include <context_base.h>
#include <map_util.h>
#include <logdefs.h>
#include <ipc_timeout.h>
#include <l4_types.h>
#include <tb_entry.h>
#include <l4_buf_iter.h>
#include <entry.h>
#include <system_clock.h>

#include <cassert>

class Thread;
class Syscall_frame;

typedef Context_ptr_base<Thread> Thread_ptr;

class Thread_ipc_base
{
public:
  Trap_state *utcb_handler_ts() const
  { return reinterpret_cast<Trap_state *>(_utcb_handler); }

protected:
  using Rcv_state = Context::Rcv_state;

  void *_utcb_handler = nullptr;

  struct Check_sender
  {
    enum R
    {
      Ok = 0,
      Open_wait_flag = 0x1,
      Queued = 2,
      Failed = 5,
    };

    static_assert((unsigned)Rcv_state::Open_wait_flag == (unsigned)Open_wait_flag,
                  "Rcv_state and Check_sender flags must be compatible");

    R s;

    constexpr Check_sender(Rcv_state s)
    : s((R)(s.s & Open_wait_flag))
    {}

    constexpr Check_sender(R s) noexcept : s(s) {}
    Check_sender() = default;

    constexpr bool is_ok() const { return !(s & ~1u); }
    constexpr bool is_open_wait() const { return s & Open_wait_flag; }
  };

  class Buf_utcb_saver
  {
  public:
    explicit Buf_utcb_saver(Utcb const *u)
    {
      buf_desc = u->buf_desc;
      buf[0] = u->buffers[0];
      buf[1] = u->buffers[1];
    }

    void restore(Utcb *u) const
    {
      u->buf_desc = buf_desc;
      u->buffers[0] = buf[0];
      u->buffers[1] = buf[1];
    }

  private:
    L4_buf_desc buf_desc;
    Mword buf[2];
  };

  /**
   * Save critical contents of UTCB during nested IPC.
   */
  class Pf_msg_utcb_saver : public Buf_utcb_saver
  {
  public:
    explicit Pf_msg_utcb_saver(Utcb const *u) : Buf_utcb_saver(u)
    {
      msg[0] = u->values[0];
      msg[1] = u->values[1];
    }

    void restore(Utcb *u) const
    {
      Buf_utcb_saver::restore(u);
      u->values[0] = msg[0];
      u->values[1] = msg[1];
    }

  private:
    Mword msg[2];
  };

  struct Log_pf_invalid : public Tb_entry
  {
    Mword pfa;
    Cap_index cap_idx;
    Mword err;
    void print(String_buffer *buf) const
    {
      buf->printf("InvCap C:%lx pfa=%lx err=%lx",
                  cxx::int_value<Cap_index>(cap_idx), pfa, err);
    }
  };

  struct Log_exc_invalid : public Tb_entry
  {
    Cap_index cap_idx;
    void print(String_buffer *buf) const
    {
      buf->printf("InvCap C:%lx", cxx::int_value<Cap_index>(cap_idx));
    }
  };
};

template<typename THREAD>
class Thread_ipc :
  public Sender,
  public Thread_ipc_base
{
private:
  using Thread = THREAD;

  Thread *_this() { return static_cast<Thread *>(this); }
  Thread const *_this() const { return static_cast<Thread const *>(this); }

  template<typename T>
  Thread *_thread(T *t)
  { return t; }

  template<typename T>
  Thread const *_thread(T const *t)
  { return t; }

  static void clear_fpu_before_receive(Thread *partner)
  {
    if (partner->_utcb_handler
        || partner->utcb().access()->inherit_fpu())
      partner->spill_fpu_if_owner();
  }

  static void set_partner_ready(Thread *partner)
  {
    partner->state.change(~Thread_ipc_mask, Thread_ready);
    if (partner->home_cpu() == current_cpu() && current() != partner)
      Sched_context::rq.current().ready_enqueue(partner->sched());
  }

  static bool vcpu_exception_upcall(Thread *t, Trap_state *ts)
  {
    Vcpu_state *vcpu = t->vcpu_state().access();

    if (!t->vcpu_exceptions_enabled(vcpu))
      return true;

    if (t->exception_triggered())
      return false;

    // before entering kernel mode to have original fpu state before
    // enabling fpu
    t->save_fpu_state_to_utcb(ts, t->utcb().access());

    t->spill_user_state();

    if (t->vcpu_enter_kernel_mode(vcpu))
      {
        // enter_kernel_mode has switched the address space from user to
        // kernel space, so reevaluate the address of the VCPU state area
        vcpu = t->vcpu_state().access();
      }

    LOG_TRACE("VCPU events", "vcpu", t, Vcpu_log,
        l->type = 2;
        l->state = vcpu->saved_state();
        l->ip = ts->ip();
        l->sp = ts->sp();
        l->trap = ts->trapno();
        l->err = ts->error();
        l->space = t->vcpu_user_space() ? static_cast<Task*>(t->vcpu_user_space())->dbg_id() : ~0;
        );
    vcpu->_regs.s = *ts;
    Entry::vcpu_return_to_kernel(t, vcpu->_entry_ip, vcpu->_sp, t->vcpu_state().usr().get());
    // does not return

    return false; // this is a dummy
 }

  static
  L4_error map_one_item(Thread *snd, L4_msg_item item, L4_fpage sfp,
                        Ref_ptr<Task> const &receiver_t,
                        L4_buf_iter::Item const *buf,
                        Utcb *rcv_utcb, Kobject::Reap_list *rl);

private:
  // Used by Thread::ipc_send_msg().
  L4_msg_tag _snd_msg_tag;
  Mword _from_spec;
  // Used when the IPC receiver executes ipc_send_msg() in the context of the
  // next sender. Otherwise we can use `rights` from `do_ipc()` directly.
  L4_fpage::Rights _ipc_send_rights;

protected:
  // More ipc state
  Thread_ptr _pager{Thread_ptr::Invalid};
  Thread_ptr _exc_handler{Thread_ptr::Invalid};

public:
  void ipc_send_msg(Context *receiver, Sender *set_closed_wait) override;
  void modify_label(Mword const *todo, int cnt) override;
  static bool transfer_msg_items(L4_msg_tag tag, Thread* snd, Utcb *snd_utcb,
                                 Thread *rcv, Utcb *rcv_utcb,
                                 L4_fpage::Rights rights);

  void set_ipc_from_spec(Mword from_spec, L4_fpage::Rights rights, bool do_set = true)
  {
    if (!do_set)
      return;

    _from_spec = from_spec;
    _ipc_send_rights = rights;
  }

  void set_snd_msg_tag(L4_msg_tag tag) { _snd_msg_tag = tag; }

  void do_ipc(L4_msg_tag tag, Thread *partner,
              Ipc_flags flags, Sender *sender, L4_timeout_pair t,
              Syscall_frame *regs);

  void do_ipc_recv(L4_msg_tag tag, Sender *sender, L4_timeout_pair t,
                   Syscall_frame *regs);

  void do_ipc_open_wait(L4_msg_tag tag, L4_timeout_pair t,
                        Syscall_frame *regs);

  void do_send_ipc(Thread *ct, L4_obj_ref self, Syscall_frame *f, Utcb *)
  {
    bool open_wait = self.op() & L4_obj_ref::Ipc_open_wait;
    ct->do_ipc(f->tag(), _this(), Ipc_flags(self.have_recv(), open_wait), open_wait ? nullptr : _this(),
               f->timeout(), f);
  }

  bool handle_page_fault_pager(Address pfa, Mword error_code,
                               L4_msg_tag::Protocol protocol);

  /* return 1 if exception could be handled
   * return 0 if not for send_exception and halt thread
   */
  int send_exception(Trap_state *ts)
  {
    assert(cpu_lock.test());

    if (!vcpu_exception_upcall(_this(), ts))
      return 1;

    L4_fpage::Rights rights = L4_fpage::Rights(0);
    Kobject_iface *handler = _exc_handler.ptr(_this()->space(), &rights);

    if (EXPECT_FALSE(!handler))
      {
        /* no exception handler (anymore), put thread to sleep */
        LOG_TRACE("Exception invalid handler", "ieh", _this(), Log_exc_invalid,
                  l->cap_idx = _exc_handler.raw());
        if (EXPECT_FALSE(_this()->space()->is_sigma0()))
          {
            ts->dump();
            panic("Sigma0 raised an exception");
          }

        handler = _this(); // block on ourselves
      }

    _this()->state.change(~Thread_cancel, Thread_in_exception);

    return exception(handler, ts, rights);
  }

private:

  void _do_ipc(L4_msg_tag tag, Thread *partner,
               Ipc_flags flags, Sender *sender, L4_timeout_pair t,
               Syscall_frame *regs);

  bool exception(Kobject_iface *handler, Trap_state *ts, L4_fpage::Rights rights);

  class Rcv_side_item
  {
  private:
    Mword *_i;

  public:
    constexpr explicit Rcv_side_item(Mword *item) : _i(item) {}

    constexpr void copy_snd_item(L4_msg_item item, L4_fpage sfp) const
    {
      _i[0] = (item.raw() & ~0x0ff6) | (sfp.raw() & 0x0ff0);
    }

    constexpr void local_fpage_received(L4_fpage sfp) const
    {
      _i[0] |= 6;
      _i[1] = sfp.raw();
    }

    constexpr void obj_id_received(Mword id_n_rights) const
    {
      _i[0] |= 4;
      _i[1] = id_n_rights;
    }

    constexpr void flag_empty_map() const
    {
      _i[0] |= 2;
    }
  };

  static bool
  try_transfer_local_id(L4_buf_iter::Item const *const buf,
                        L4_fpage sfp, Rcv_side_item rcv_item, Thread* snd,
                        Thread *rcv)
  {
    if (buf->b.is_rcv_id())
      {
        if (snd->space() == rcv->space())
          {
            rcv_item.local_fpage_received(sfp);
            return true;
          }
        else
          {
            Obj_space::Capability cap = snd->space()->lookup(sfp.obj_index());
            Kobject_iface *o = cap.obj();
            if (EXPECT_TRUE(o && o->is_local(rcv->space())))
              {
                Mword rights = cap.rights()
                               & cxx::int_value<L4_fpage::Rights>(sfp.rights());
                rcv_item.obj_id_received(o->obj_id() | rights);
                return true;
              }
          }
      }
    return false;
  }

  [[nodiscard]] static bool
  copy_utcb_to_utcb(L4_msg_tag tag, Thread *snd, Thread *rcv,
                    L4_fpage::Rights rights)
  {
    assert (cpu_lock.test());

    Utcb *snd_utcb = snd->utcb().access();
    Utcb *rcv_utcb = rcv->utcb().access();
    Mword s = tag.words();
    Mword r = Utcb::Max_words;

    Mem::memcpy_mwords(rcv_utcb->values, snd_utcb->values, r < s ? r : s);

    bool success = true;
    if (tag.items())
      success = transfer_msg_items(tag, snd, snd_utcb, rcv, rcv_utcb, rights);

    if (success
        && tag.transfer_fpu()
        && rcv_utcb->inherit_fpu()
        && (rights & L4_fpage::Rights::CS()))
      snd->transfer_fpu(rcv);

    return success;
  }

  [[nodiscard]] bool
  copy_utcb_to(L4_msg_tag tag, Thread* receiver,
                       L4_fpage::Rights rights)
  {
    // we cannot copy trap state to trap state!
    assert (!_this()->_utcb_handler || !receiver->_utcb_handler);
    if (EXPECT_FALSE(this->_utcb_handler != 0))
      return _this()->copy_ts_to_utcb(tag, _this(), receiver, rights);
    else if (EXPECT_FALSE(receiver->_utcb_handler != 0))
      return _this()->copy_utcb_to_ts(tag, _this(), receiver, rights);
    else
      return copy_utcb_to_utcb(tag, _this(), receiver, rights);
  }

  bool transfer_msg(L4_msg_tag tag, Thread *receiver)
  {
    Syscall_frame* dst_regs = receiver->rcv_regs();

    auto rights = _ipc_send_rights;
    bool success = copy_utcb_to(tag, receiver, rights);
    tag.set_error(!success);
    dst_regs->tag(tag);
    dst_regs->from(_from_spec);

    // setup the reply capability in case of a call
    if (success && _this()->is_partner(receiver))
      receiver->set_reply_cap(_this(), rights);

    return success;
  }

  bool are_vcpu_irqs_enabled() const
  {
    Thread_state state = _this()->state();
    if (!state.vcpu_enabled())
      return false;

    return _this()->vcpu_state().access()->irqs_enabled();
  }

  Sender *next_sender(Sender *sender)
  {
    if (sender) // closed wait
      {
        if (Sender::in_sender_list(sender, _this()->sender_list()))
          return sender;
        return nullptr;
      }

    // open wait
    if (auto *next = Sender::cast(_this()->sender_list()->first()))
      return next;

    return nullptr;
  }

  Sender *dequeue_next_sender(Sender *next)
  {
    if (EXPECT_FALSE(!Sender::in_sender_list(next, _this()->sender_list())))
      return nullptr;

    if (_this()->sender_list()->dequeue(next->qitem()))
      {
        _this()->set_partner(next);
        return next;
      }

    return nullptr;
  }

  bool activate_ipc_partner(Thread *partner, Cpu_number current_cpu,
                            bool do_switch)
  {
    // the existence of 'partner' is ensured by the cpu_lock being held
    // continuously from the transfer_msg call through here (RCU quiescent
    // states require IRQs enabled without cpu_lock).
    partner->state.change(~Thread_receive_in_progress, Thread_ready);
    if (partner->home_cpu() == current_cpu)
      {
        if (do_switch)
          {
            _this()->schedule_if(_this()->switch_exec_locked(
                  partner, Context::Not_Helping) != Context::Switch::Ok);
            return true;
          }
        else
          return _this()->deblock_and_schedule(partner);
      }

    partner->remote_ready_enqueue();
    return false;
  }

  void do_receive_wait(Sender *closed_sender)
  {
    if (closed_sender == this)
      _this()->switch_sched(_this()->sched(), &Sched_context::rq.current());

    if (!_this()->state.has(Thread_ready))
      _this()->schedule();
  }

  bool try_receive(Sender *&next, Sender *closed_sender, L4_timeout rcv_to, IPC_timeout &rcv_timeout)
  {
    Mword rcv_state;
    auto test_in_progress = [&rcv_state](Mword s) -> bool
      {
        if (s & Thread_ipc_abort_mask)
          {
            rcv_state = 0;
            return false;
          }

        rcv_state = s & Thread_ipc_receive_mask;
        return s & Thread_receive_in_progress;
      };

    // block while receiving a message is in progress
    if (_this()->state.change_if(test_in_progress, ~Thread_ready, 0))
      {
        do_receive_wait(closed_sender);
        return true;
      }

    if (!rcv_state)
      return true;

    if (!next && !(next = next_sender(closed_sender)))
      {
        // no appropriate sender in the queue, check/set timeout or fail IPC
        if (rcv_timeout.has_hit())
          return true;

        if (!rcv_timeout.is_set() && !_this()->setup_timer(rcv_to, _this()->utcb().access(true), &rcv_timeout))
          return true;

        auto test_waiting = [&rcv_state](Mword s) -> bool
          {
            if (s & Thread_ipc_abort_mask)
              {
                rcv_state = 0;
                return false;
              }

            rcv_state = s & Thread_ipc_receive_mask;
            return s & Thread_receive_wait;
          };

        if (_this()->state.change_if(test_waiting, ~Thread_ready, 0))
          {
            do_receive_wait(closed_sender);
            return true;
          }

        if (!rcv_state)
          return true;

        return false;
      }

    if (!(next = dequeue_next_sender(next)))
      return false;

    if (_this()->state.change_safely(~Thread_receive_wait, Thread_receive_in_progress))
      {
        // Receive timeout might already been hit here if the next sender was
        // queued after we activated the IPC partner. In that case ignore the
        // timeout (clear the timeout flag) and transfer the message from the
        // pending sender anyway.
        _this()->state.del(Thread_timeout);
        //_this()->state.change_dirty(~(Thread_ipc_mask | Thread_timeout), Thread_receive_in_progress);
        _this()->vcpu_update_state();
        next->ipc_send_msg(_this());
        _this()->state.del(Thread_ipc_mask);
        return true;
      }

    next->sender_enqueue(_this()->sender_list(), next->sender_prio());
    if (!_this()->state.has(Thread_ipc_receive_mask))
      return true;

    next = nullptr;
    return false;
  }

  void do_receive(Sender *next, Sender *closed_sender, L4_timeout rcv_to, IPC_timeout &rcv_timeout)
  {
    while (!try_receive(next, closed_sender, rcv_to, rcv_timeout))
      ;
  }

  void set_ipc_error(L4_error e, Thread *rcv)
  {
    _this()->utcb().access()->error = e;
    rcv->utcb().access()->error = L4_error(e, L4_error::Rcv);
  }

  /**
   * \pre Runs on the sender CPU
   * \retval true when the IPC was aborted
   * \retval false iff the IPC was already finished
   */
  bool abort_send(L4_error e, Thread *partner)
  {
    if (sender_dequeue(partner->sender_list()))
      {
        if (partner->home_cpu() == current_cpu())
          partner->vcpu_update_state();

        _this()->state.del(Thread_full_ipc_mask);
        _this()->utcb().access()->error = e;
        return true;
      }

    while (_this()->state.change_if([](Mword s) { return s & Thread_send_in_progress; }, ~Thread_ready, 0))
      {
        // FIXME: should tell the partner/receiver to stop actively receiving
        _this()->schedule();
      }

    _this()->state.del(Thread_full_ipc_mask);
    // IPC done, nothing to abort
    return false;
  }

  /**
   * \pre Runs on the sender CPU
   * \retval true iff the IPC was finished during the wait
   * \retval false iff the IPC was aborted with some error
   */
  bool do_send_wait(Thread *partner, L4_timeout snd_t)
  {
    IPC_timeout timeout;

    if (EXPECT_FALSE(snd_t.is_finite()))
      {
        Unsigned64 system_clock = System_clock::clock();
        Unsigned64 tval = snd_t.microsecs(system_clock, _this()->utcb().access(true));
        // Zero timeout or timeout expired already -- give up
        if (tval == 0 || tval <= system_clock)
          return !abort_send(L4_error::Timeout, partner);

        _this()->set_timeout(&timeout, tval);
      }

    Mword ipc_state;
    auto test_state = [&ipc_state](Mword s)
      {
        ipc_state = s & (Thread_send_wait | Thread_ipc_abort_mask);
        return ipc_state == Thread_send_wait;
      };

    while (_this()->state.change_if(test_state, ~Thread_ready, 0))
      _this()->schedule();

    _this()->reset_timeout();

    if (EXPECT_TRUE(ipc_state == 0))
      return true;

    if (EXPECT_FALSE(ipc_state & Thread_transfer_failed))
      {
        _this()->state.del(Thread_full_ipc_mask);
        return false;
      }

    if (EXPECT_FALSE(ipc_state & Thread_cancel))
      return !abort_send(L4_error::Canceled, partner);

    if (EXPECT_FALSE(ipc_state & Thread_timeout))
      return !abort_send(L4_error::Timeout, partner);

    return true;
  }

  Check_sender
  check_send_fail(L4_error error)
  {
    _this()->utcb().access()->error = error;
    return Check_sender::Failed;
  }

  Check_sender
  check_send(L4_msg_tag snd_tag, Thread *receiver, bool zero_timeout, bool cpu_local)
  {
    if (EXPECT_FALSE(receiver->is_invalid()))
      return check_send_fail(L4_error::Not_existent);

    if (auto ok = receiver->sender_ok(_this(), cpu_local))
      if (receiver->state.change_safely(~Thread_receive_wait, Thread_receive_in_progress))
        return ok;

    if (zero_timeout)
      return check_send_fail(L4_error::Timeout);

    // set _snd_msg_tag to enable active receiving
    _snd_msg_tag = snd_tag;
    Mem::mp_release();
    for (;;)
      {
        _this()->state.add(Thread_send_wait);
        if (EXPECT_FALSE(!_this()->sender_enqueue(receiver->sender_list(),
                                                  _this()->sched_context()->prio())))
          return check_send_fail(L4_error::Not_existent);

        if (auto ok = receiver->sender_ok(_this(), cpu_local))
          {
            if (!_this()->sender_dequeue(receiver->sender_list()))
              return Check_sender::Queued;

            if (receiver->state.change_safely(~Thread_receive_wait, Thread_receive_in_progress))
              {
                _this()->state.del(Thread_send_wait);
                return ok;
              }
          }
        else
          break;
      }

    receiver->vcpu_set_irq_pending();
    return Check_sender::Queued;
  }


  inline bool
  _ipc_send(L4_msg_tag tag, Thread *partner,
            Ipc_flags flags, L4_timeout_pair t,
            Syscall_frame *regs,
            Cpu_number current_cpu);
};

/**
 * Receiver-ready callback for a Thread sender.
 *
 * Called in the receiver's context. Transfers the message and transitions the
 * sender to ready (or receive-wait if a reply phase follows).
 *
 * If `set_closed_wait` is non-null and the sender has a receive phase with a
 * currently-set Poi (indicating a pending closed wait), the Poi is redirected
 * to `set_closed_wait`. This allows the WQ dispatch path to bind the sender's
 * reply wait to the specific receiver thread that served the request, ensuring
 * only that receiver's reply cap satisfies the closed wait.
 *
 * `Thread_send_in_progress` is set before `transfer_msg` so that
 * `abort_send` can detect the active-transfer window and spin rather than
 * dequeue the sender prematurely.
 */
template<typename THREAD>
void
Thread_ipc<THREAD>::ipc_send_msg(Context *receiver, Sender *set_closed_wait)
{
  if (EXPECT_FALSE(_this()->home_cpu() != receiver->home_cpu()
        && _snd_msg_tag.transfer_fpu()))
    clear_fpu_before_receive(nonull_static_cast<Thread*>(receiver));

  if (EXPECT_FALSE(set_closed_wait != nullptr && _this()->sender_list()->current_poi()))
    _this()->set_partner(set_closed_wait);

  _this()->state.add(Thread_send_in_progress);
  bool success = transfer_msg(_snd_msg_tag, nonull_static_cast<Thread*>(receiver));

  Mword state_del;
  Mword state_add;
  if (EXPECT_TRUE(success))
    {
      state_del = Thread_ipc_mask;
      state_add = Thread_ready;
      if (_this()->rcv_prepared())
        state_add |= Thread_receive_wait;
    }
  else
    {
      state_del = 0;
      state_add = Thread_transfer_failed | Thread_ready;
    }

  _this()->state.change(~state_del, state_add);
  // state_add always has ready set, so unconditionally enqueue
  if (_this()->xcpu_lazy_ready_enqueue())
    receiver->switch_to_locked(_this());
}

template<typename T>
void
Thread_ipc<T>::modify_label(Mword const *todo, int cnt)
{
  Mword l = _from_spec;
  for (int i = 0; i < cnt*4; i += 4)
    {
      Mword const test_mask = todo[i];
      Mword const test      = todo[i+1];
      if ((l & test_mask) == test)
        {
          Mword const del_mask = todo[i+2];
          Mword const add_mask = todo[i+3];

          l = (l & ~del_mask) | add_mask;
          _from_spec = l;
          return;
        }
    }
}

template<typename THREAD>
inline L4_error
Thread_ipc<THREAD>::map_one_item(Thread *snd, L4_msg_item item, L4_fpage sfp,
                                 Ref_ptr<Task> const &receiver_t,
                                 L4_buf_iter::Item const *buf,
                                 Utcb *rcv_utcb, Kobject::Reap_list *rl)
{
  Kobject::Locked<Task> rcv_t;
  if (EXPECT_FALSE(buf->b.compound()))
    {
      unsigned cap_br = buf->b.cap_br_idx();
      if (cap_br >= Utcb::Max_buffers)
        return L4_error::Overflow;

      L4_obj_ref tc(rcv_utcb->buffers[cap_br]);
      if (EXPECT_FALSE(!tc.valid()))
        return L4_error::Overflow;

      auto task_ref = receiver_t->lookup_local(tc.cap(), L4_fpage::Rights::CS());
      rcv_t = Kobject::Locked<Task>(task_ref.as<Task>());
    }
  else
    rcv_t = Kobject::Locked<Task>(receiver_t);

  if (EXPECT_FALSE(!rcv_t))
    return L4_error::Overflow;

  // Take the existence_lock for synchronizing maps -- kind of
  // coarse-grained. We could go for ref counting and more
  // fine-grained locks internally...

  auto c_lock = lock_guard<Lock_guard_inverse_policy>(cpu_lock);
  return fpage_map(snd->space(), sfp, rcv_t.get(), L4_fpage(buf->d), item, rl);
}

template<typename THREAD>
bool
Thread_ipc<THREAD>::transfer_msg_items(L4_msg_tag tag,
    Thread* snd, Utcb *snd_utcb,
    Thread *rcv, Utcb *rcv_utcb,
    L4_fpage::Rights rights)
{
  Ref_ptr<Task> receiver_t(nonull_static_cast<Task*>(rcv->space()));
  L4_buf_iter mem_buffer(rcv_utcb, rcv_utcb->buf_desc.mem());
  L4_buf_iter io_buffer(rcv_utcb, rcv_utcb->buf_desc.io());
  L4_buf_iter obj_buffer(rcv_utcb, rcv_utcb->buf_desc.obj());
  L4_snd_item_iter snd_item(snd_utcb, tag.words());
  int items = tag.items();
  Mword *rcv_word = rcv_utcb->values + tag.words();

  Kobject::Reap_list rl;

  while (items > 0 && snd_item.more())
    {
      if (EXPECT_FALSE(!snd_item.next()))
        {
          snd->set_ipc_error(L4_error::Overflow, rcv);
          return false;
        }

      L4_snd_item_iter::Item const *const item = snd_item.get();

      if (item->b.is_void())
        { // XXX: not sure if void fpages are needed
          // skip send item and current rcv_buffer
          --items;
          *rcv_word = 0;
          rcv_word += 2;
          continue;
        }

      L4_buf_iter *buf_iter = nullptr;

      switch (item->b.type())
        {
        case L4_msg_item::Map:
          switch (L4_fpage(item->d).type())
            {
            case L4_fpage::Memory: buf_iter = &mem_buffer; break;
            case L4_fpage::Io:     buf_iter = &io_buffer; break;
            case L4_fpage::Obj:    buf_iter = &obj_buffer; break;
            default: break;
            }
          break;
        default:
          break;
        }

      if (EXPECT_FALSE(!buf_iter))
        {
          snd->set_ipc_error(L4_error::Overflow, rcv);
          return false;
        }

      L4_buf_iter::Item const *const buf = buf_iter->get();

      if (EXPECT_FALSE(buf->b.is_void() || buf->b.type() != item->b.type()))
        {
          snd->set_ipc_error(L4_error::Overflow, rcv);
          return false;
        }

        {
          assert (item->b.type() == L4_msg_item::Map);
          L4_fpage sfp(item->d);

          Rcv_side_item rcv_item(rcv_word);

          rcv_item.copy_snd_item(item->b, sfp);

          rcv_word += 2;

          // diminish when sending via restricted IPC gates
          if (sfp.type() == L4_fpage::Obj)
            sfp.mask_rights(rights | L4_fpage::Rights::CRW() | L4_fpage::Rights::CD());

          if (!try_transfer_local_id(buf, sfp, rcv_item, snd, rcv))
            {
              // we need to do a real mapping
              L4_error err = map_one_item(snd, item->b, sfp, receiver_t, buf, rcv_utcb, &rl);
              if (EXPECT_FALSE(!err.ok()))
                {
                  snd->set_ipc_error(err, rcv);
                  return false;
                }

              if (err.empty_map())
                rcv_item.flag_empty_map();
            }
        }

      --items;

      if (!item->b.compound())
        buf_iter->next();
    }

  if (EXPECT_FALSE(items))
    {
      snd->set_ipc_error(L4_error::Overflow, rcv);
      return false;
    }

  return true;
}

template<typename T>
inline bool
Thread_ipc<T>::_ipc_send(L4_msg_tag tag, Thread *partner,
                         Ipc_flags flags, L4_timeout_pair t,
                         Syscall_frame *regs,
                         Cpu_number current_cpu)
{
  bool ok;
  bool activate_partner = false;
  Check_sender result = check_send(tag, partner, t.snd.is_zero(),
                                   EXPECT_TRUE(current_cpu == partner->home_cpu()));
  switch (result.s)
    {
    case Check_sender::Queued:
      if (partner->home_cpu() != current_cpu && partner->are_vcpu_irqs_enabled())
        partner->set_xcpu_ipc_pending();

      // --- do_send_wait is blocking... RCU references need protection.
      ok = _this()->do_send_wait(partner, t.snd); // --- blocking point ---

      // --- partner is invalid here
      // FIXME: sender might be gone from here
      break;

    case Check_sender::Failed:
      _this()->state.del(Thread_ipc_mask);
      ok = false;
      break;

    default:
      // ping pong with timeouts will profit from resetting the receiver´s
      // timeout, because it will require much less sorting overhead. If we
      // don't reset the timeout, the probability is very high that the
      // receiver timeout is in the timeout queue.
      if (EXPECT_TRUE(current_cpu == partner->home_cpu()))
        partner->reset_timeout();

      // --- transfer is possibly blocking... RCU references need protection.
      ok = transfer_msg(tag, partner);
      partner->state.del(Thread_receive_in_progress);
      // --- from here partner is valid until the next preemption point
      // FIXME: sender might be gone already

      // switch to receiving state
      Mword state_to_add = 0;
      if (ok && flags.have_receive())
        state_to_add = Thread_receive_wait;

      _this()->state.change(~Thread_ipc_mask, state_to_add);
      activate_partner = partner != this;
      break;
    }

  if (EXPECT_FALSE(!ok))
    {
      // Send failed. Skip the receive phase (Thread_receive_wait was not
      // set) but still activate the partner (may include a switch to it)
      // to inform the partner about the failed IPC.
      regs->tag(L4_msg_tag::error());
    }

  return activate_partner;
}

/**
 * Send an IPC message and/or receive an IPC message.
 *
 * \param tag           message tag; specifies details about the send phase
 * \param from_spec     label for the receiver
 * \param partner       communication partner in the send phase; nullptr if
 *                      there is no send phase
 * \param have_receive  enable/disable receive phase
 * \param sender        communication partner in the receive phase; use
 *                      `nullptr` to accept any communication partner (open
 *                      wait)
 * \param t             timeouts for send phase and receive phase
 * \param regs          IPC registers of the initiator of this IPC
 * \param rights        object permissions; usually the permissions of the
 *                      invoked capability during a syscall
 *
 * \pre `cpu_lock` must be held
 * \pre may only be called on current_thread()
 *
 * This function blocks until the message can be sent/received, the respective
 * timeout hits, the IPC is canceled, or the thread is killed.
 *
 * \todo review closed wait handling of sender during possible
 *       quiescent states and blocking.
 */
template<typename T>
inline void
Thread_ipc<T>::_do_ipc(L4_msg_tag tag, Thread *partner,
                       Ipc_flags flags, Sender *sender, L4_timeout_pair t,
                       Syscall_frame *regs)
{
  assert (cpu_lock.test());
  assert (_this() == current());

  bool do_switch = false;

  assert (!_this()->state.has(Thread_ipc_mask));

  _this()->prepare_receive(flags, sender, regs);
  bool activate_partner = false;

  if (partner)
    {
      // this resets the reply capability even if it was not used
      // for this reply, but the message is sent via some other
      // capability
      _this()->reset_caller(partner);

      assert(!in_sender_list());
      do_switch = tag.do_switch();
      activate_partner = _ipc_send(tag, partner, flags, t, regs, ::current_cpu());
    }
  else
    {
      assert (flags.have_receive());
      _this()->state.add(Thread_receive_wait);
    }

  {
    IPC_timeout rcv_timeout;
    // Indicates whether the receive timeout had already expired (is in the past
    // or is zero) when attempting to set up the timer for it.
    bool rcv_timeout_expired = false;

    // Holds the next sender if the IPC has a receive phase.
    Sender *next = nullptr;

    bool rcv_in_progress = _this()->state() & Thread_ipc_receive_mask;

    if (flags.have_receive() && rcv_in_progress)
      {
        assert (!in_sender_list());
        assert (!_this()->state.has(Thread_send_wait));
        next = next_sender(sender);

        if (!next)
          // If there is no next sender yet, we have to set up the receive
          // timeout, either for a direct switch to the IPC partner or for a
          // do_receive_wait.
          rcv_timeout_expired = !_this()->setup_timer(t.rcv, _this()->utcb().access(true), &rcv_timeout);
      }

    if (activate_partner)
      {
        // Directly switch to the IPC partner (receiver) only if all of the
        // following apply:
        //  - The Schedule flag is not set in the message tag, i.e. the sender
        //    is willing to donate its remaining time-slice to the receiver
        //    (provided by `do_switch`).
        //  - The home CPU of the receiver is the current CPU (provided by
        //    `do_switch`).
        //  - If the IPC has a receive phase, there must be no next sender
        //    already pending (next != nullptr implicates a receive phase) and
        //    the receive timeout must not have expired yet.
        //  - The IPC transfer qualifies for time-slice donation, i.e. the
        //    sender transitions into a closed wait (call) or the sender runs
        //    on a foreign scheduling context.
        bool do_direct_switch = do_switch && !next && !rcv_timeout_expired
          && (   (rcv_in_progress && sender) // closed wait (call)
              || (Sched_context::rq.current().current_sched() != _this()->sched()));

        activate_ipc_partner(cxx::move(partner), ::current_cpu(), do_direct_switch);
        // --- partner no longer valid from this point ...
      }

    if (flags.have_receive() && rcv_in_progress)
      do_receive(next, sender, t.rcv, rcv_timeout);
  }

  Mword state = _this()->state();

  if (EXPECT_TRUE (!(state & Thread_full_ipc_mask)))
    return;

  if (state & Thread_ipc_mask)
    {
      if (flags.have_receive() && sender && sender == partner)
        _this()->reset_partner_reply_cap();

      Utcb *utcb = _this()->utcb().access(true);
      // the IPC has not been finished.  could be timeout or cancel
      // XXX should only modify the error-code part of the status code

      if (EXPECT_FALSE(state & Thread_cancel))
        {
          // we've presumably been reset!
          regs->tag(Kobject::commit_error(utcb, L4_error::R_canceled, regs->tag()));
        }
      else
        regs->tag(Kobject::commit_error(utcb, L4_error::R_timeout, regs->tag()));
    }
  _this()->state.del(Thread_full_ipc_mask);
}

#ifdef FIASCO_THREAD_IMPL

template<typename T>
FIASCO_FLATTEN
void
Thread_ipc<T>::do_ipc_recv(L4_msg_tag tag, Sender *sender, L4_timeout_pair t,
                           Syscall_frame *regs)
{
  _do_ipc(tag, nullptr, Ipc_flags(true, !sender), sender, t, regs);
}

template<typename T>
FIASCO_FLATTEN
void
Thread_ipc<T>::do_ipc(L4_msg_tag tag, Thread *partner,
                      Ipc_flags flags, Sender *sender, L4_timeout_pair t,
                      Syscall_frame *regs)
{
  _do_ipc(tag, partner, flags, sender, t, regs);
}

#endif

template<typename T>
FIASCO_FLATTEN inline
void
Thread_ipc<T>::do_ipc_open_wait(L4_msg_tag tag, L4_timeout_pair t,
                                Syscall_frame *regs)
{
  _do_ipc(tag, nullptr, Ipc_flags(true, true), nullptr, t, regs);
}

/** Page fault handler.
    This handler suspends any ongoing IPC, then sets up page-fault IPC.
    Finally, the ongoing IPC's state (if any) is restored.
    @param pfa page-fault virtual address
    @param error_code page-fault error code.
 */
template<typename T> bool
Thread_ipc<T>::handle_page_fault_pager(Address pfa, Mword error_code,
                                       L4_msg_tag::Protocol protocol)
{
  auto guard = lock_guard(cpu_lock);

  L4_fpage::Rights rights;
  Kobject_iface *pager = _pager.ptr(_this()->space(), &rights);

  if (!pager)
    {
      WARN("CPU%u: Pager of %lx is invalid (pfa=" L4_PTR_FMT
           ", errorcode=" L4_PTR_FMT ") to %lx (pc=%lx)\n",
           cxx::int_value<Cpu_number>(current_cpu()), _this()->dbg_id(), pfa,
           error_code, cxx::int_value<Cap_index>(_pager.raw()), _this()->regs()->ip());


      LOG_TRACE("Page fault invalid pager", "ipfh", _this(), Log_pf_invalid,
                l->cap_idx = _pager.raw();
                l->err     = error_code;
                l->pfa     = pfa);

      _this()->kill();
      return true;
    }

  // set up a register block used as an IPC parameter block for the
  // page fault IPC
  Syscall_frame r;

  // save the UTCB fields affected by PF IPC
  Mword vcpu_irqs = _this()->vcpu_disable_irqs();
  Mem::barrier();
  Utcb *utcb = _this()->utcb().access(true);
  Pf_msg_utcb_saver saved_utcb_fields(utcb);


  utcb->buf_desc = L4_buf_desc(0, 0, 0, L4_buf_desc::Inherit_fpu);
  utcb->buffers[0] = L4_msg_item::map().raw();
  utcb->buffers[1] = L4_fpage::all_spaces().raw();

  utcb->values[0] = PF::addr_to_msgword0(pfa, error_code);
  utcb->values[1] = _this()->regs()->ip(); //PF::pc_to_msgword1 (regs()->ip(), error_code));

  L4_timeout_pair timeout(L4_timeout::Never, L4_timeout::Never);

  L4_msg_tag tag(2, 0, 0, protocol);

  r.timeout(timeout);
  r.tag(tag);
  r.from(0);
  r.ref(L4_obj_ref(_pager.raw(), L4_obj_ref::Ipc_call_ipc));
  pager->invoke(r.ref(), rights, &r, utcb);


  bool success = true;

  if (EXPECT_FALSE(r.tag().has_error()))
    {
      if (utcb->error.snd_phase()
          && (utcb->error.error() == L4_error::Not_existent)
          && PF::is_usermode_error(error_code)
          && !_this()->state.has(Thread_cancel))
        {
          success = false;
        }
    }
  else // no error
    {
      // If the pager rejects the mapping, it replies -1 in msg.w0
      if (EXPECT_FALSE (r.tag().proto() < 0 || utcb->values[0] == Mword(-1)))
        success = false;
    }

  // restore previous IPC state

  saved_utcb_fields.restore(utcb);
  Mem::barrier();
  _this()->vcpu_restore_irqs(vcpu_irqs);
  return success;
}

/**
 * \pre must run with local IRQs disabled (CPU lock held)
 * to ensure that handler does not disappear meanwhile.
 */
template<typename T>
bool
Thread_ipc<T>::exception(Kobject_iface *handler, Trap_state *ts, L4_fpage::Rights rights)
{
  Syscall_frame r;
  L4_timeout_pair timeout(L4_timeout::Never, L4_timeout::Never);

  CNT_EXC_IPC;

  Mword vcpu_irqs = _this()->vcpu_disable_irqs();
  Mem::barrier();

  void *old_utcb_handler = _utcb_handler;
  _utcb_handler = ts;

  // fill registers for IPC
  Utcb *utcb = _this()->utcb().access(true);
  Buf_utcb_saver saved_state(utcb);

  utcb->buf_desc = L4_buf_desc(0, 0, 0, L4_buf_desc::Inherit_fpu);
  utcb->buffers[0] = L4_msg_item::map().raw();
  utcb->buffers[1] = L4_fpage::all_spaces().raw();

  // clear regs
  L4_msg_tag tag(L4_exception_ipc::Msg_size, 0, L4_msg_tag::Transfer_fpu,
                 L4_msg_tag::Label_exception);

  r.tag(tag);
  r.timeout(timeout);
  r.from(0);
  r.ref(L4_obj_ref(_exc_handler.raw(), L4_obj_ref::Ipc_call_ipc));
  _this()->spill_user_state();
  handler->invoke(r.ref(), rights, &r, utcb);
  _this()->fill_user_state();

  saved_state.restore(utcb);

  _this()->state.del(Thread_in_exception);

  // restore original utcb_handler
  _utcb_handler = old_utcb_handler;
  Mem::barrier();
  _this()->vcpu_restore_irqs(vcpu_irqs);

  // FIXME: handle not existing exception handler properly
  // for now, just ignore any errors
  return 1;
}


