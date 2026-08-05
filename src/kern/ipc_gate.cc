
#include "ipc_gate.h"
#include "globalconfig.h"
#include "ipc_timeout.h"
#include "kmem_slab.h"
#include "kobject_rpc.h"
#include "static_init.h"
#include <system_clock.h>

JDB_DEFINE_TYPENAME(Ipc_gate, "\033[35mGate\033[m");
static Kmem_slab_t<Ipc_gate> _ipc_gate_allocator("Ipc_gate");

using Ipc_flags = Thread::Ipc_flags;

class Ipc_gate_helper : public Ipc_gate_if
{
public:
  Mword obj_id() const override final
  { return _this()->_id.load(cxx::memory_order_acquire); }

  void initiate_deletion(Kobject ***rl) override final
  { Ipc_gate::from_gate(this)->initiate_deletion(rl); }

  Kobject_mappable* map_root() override final
  { return Ipc_gate::from_gate(this)->map_root(); }

  Kobject_iface* downgrade(long unsigned int) override final
  { return this; }

  void del(Kobject_iface *) override
  {}

  void del_notify() override
  {}

#if defined (CONFIG_JDB)
  Kobject_dbg* dbg_info() const override final
  { return Ipc_gate::from_gate(this)->dbg_info(); }
#endif // CONFIG_JDB
};

class Ipc_gate_bound final : public Ipc_gate_helper
{
public:
  Ipc_gate_bound() = default;
  Ipc_gate_bound(Thread *t, Mword id)
  {
    t->inc_ref();
    _this()->_id.store(id, cxx::memory_order_relaxed);
    _this()->_tgt.store(t, cxx::memory_order_release);
  }

  void invoke(L4_obj_ref /*self*/, L4_fpage::Rights rights,
              Syscall_frame *f, Utcb *utcb) override;

  bool is_local(Space *s) const override
  {
    auto *t = _this()->_tgt.load();
    return t && t->space() == s;
  }

  void del(Kobject_iface *o) override
  {
    auto t = nonull_static_cast<Thread *>(o);
    if (t->dec_ref() == 0)
      delete t;
  }

  void del_notify() override
  {
    _this()->target()->ipc_gate_deleted(_this()->_id.load(cxx::memory_order_acquire));
  }
};

FIASCO_FLATTEN
void
Ipc_gate_bound::invoke(L4_obj_ref, L4_fpage::Rights rights,
                 Syscall_frame *f, Utcb *utcb)
{
  //LOG_MSG_3VAL(current(), "gIPC", Mword(_thread), _id, f->obj_2_flags());
  //printf("Invoke: Ipc_gate_bound(%lx->%p)...\n", _id, _thread);
  Thread *ct = current_thread();
  Thread *sender = nullptr;
  Thread *partner = nullptr;
  bool have_rcv = false;

  Thread *t = _this()->target();
  if (EXPECT_FALSE(!t))
    {
      f->tag(commit_error(utcb, L4_error::Not_existent));
      return;
    }

  bool ipc = t->check_sys_ipc(f->ref().op(), &partner, &sender, &have_rcv);

  LOG_TRACE("IPC Gate invoke", "gate", current(), Ipc_gate::Log_ipc_gate_invoke,
      l->gate_dbg_id = Ipc_gate::from_gate(this)->dbg_id();
      l->thread_dbg_id = t->dbg_id();
      l->label = _this()->_id.load(cxx::memory_order_relaxed) | cxx::int_value<L4_fpage::Rights>(rights);
  );

  if (EXPECT_FALSE(!ipc))
    f->tag(commit_error(utcb, L4_error::Not_existent));
  else
    {
      ct->set_ipc_from_spec(_this()->_id.load(cxx::memory_order_acquire)
                            | cxx::int_value<L4_fpage::Rights>(rights), rights, partner);
      ct->do_ipc(f->tag(), partner, Ipc_flags(have_rcv, !sender), sender, f->timeout(), f);
    }
}


class Ipc_gate_unbound final : public Ipc_gate_helper
{
public:
  Ipc_gate_unbound() = default;

  void invoke(L4_obj_ref /*self*/, L4_fpage::Rights rights,
              Syscall_frame *f, Utcb *utcb) override;

  bool is_local(Space*) const override { return false; }

private:
  L4_error block(Thread *ct, L4_timeout const &to, Utcb *u)
  {
    Unsigned64 tval = 0;
    if (!to.is_never())
      {
        Unsigned64 system_clock = System_clock::clock();
        tval = to.microsecs(system_clock, u);
        if (tval == 0 || tval <= system_clock)
          return L4_error::Timeout;
      }

    ct->sender_enqueue(&Inner_gate::from_gate(this)->_wait_q, ct->sched_context()->prio());
    ct->state.change_dirty(~Thread_ready, Thread_send_wait);

    IPC_timeout timeout;
    if (tval)
      {
        timeout.set(tval, current_cpu());
        ct->set_timeout(&timeout);
      }
    // else infinite timeout

    ct->schedule();

    Mword state = ct->state.change(~Thread_full_ipc_mask, Thread_ready);
    ct->reset_timeout();

    ct->sender_dequeue(&_this()->_wait_q);

    if (state & Thread_timeout)
      return L4_error::Timeout;

    if (state & Thread_cancel)
      return L4_error::Canceled;

    return L4_error::None;
  }
};

void
Ipc_gate_unbound::invoke(L4_obj_ref, L4_fpage::Rights,
                         Syscall_frame *f, Utcb *utcb)
{
  //LOG_MSG_3VAL(current(), "gIPC", Mword(_thread), _id, f->obj_2_flags());

  Thread *ct = current_thread();

  L4_error e = block(ct, f->timeout().snd, utcb);
  if (!e.ok())
    {
      f->tag(commit_error(utcb, e));
      return;
    }

  Entry::reenter_syscall(ct);
}


Kobject_iface *
Ipc_gate::downgrade(unsigned long attr)
{
  if (attr & L4_msg_item::C_obj_right_1)
    return gate();
  else
    return this;
}

inline
L4_msg_tag
Ipc_gate::bind_thread(L4_obj_ref, L4_fpage::Rights rights,
                          Syscall_frame *f, Utcb const *in, Utcb *)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  L4_msg_tag tag = f->tag();

  if (tag.words() < 2)
    return commit_result(-L4_err::EMsgtooshort);

  auto *t = Ko::first_cap(&tag, in, L4_fpage::Rights::CS()).deref<Thread>(&tag);
  if (!t)
    return tag;

  t->inc_ref();

  _id.store(in->values[1]);
  auto *old = _tgt.load(cxx::memory_order_relaxed);
  while (!_tgt.compare_exchange_strong(old, t))
    ;

  if (!old)
    construct_gate<Ipc_gate_bound>();

  unblock_all();
  current()->rcu_wait();
  unblock_all();

  if (old)
    gate()->del(old);

  return commit_result(0);
}

inline
L4_msg_tag
Ipc_gate::get_infos(L4_obj_ref, L4_fpage::Rights,
                        Syscall_frame *, Utcb const *, Utcb *out)
{
  out->values[0] = id();
  return commit_result(0, 1);
}

void
Ipc_gate::unblock_all(bool abort)
{
  for (;;)
    {
      Prio_list_elem *h;
        {
          auto g1 = lock_guard(cpu_lock);
          h = _wait_q.dequeue_first();
        }

      if (!h)
        return;

      Thread *w = static_cast<Thread*>(Sender::cast(h));
      if (abort)
        w->state.change(~Thread_send_wait, Thread_cancel);
      w->activate();
    }
}

void
Ipc_gate::destroy(Kobject ***r)
{
  Kobject::destroy(r);
  gate()->del_notify();
  auto *tmp = _tgt.load(cxx::memory_order_acquire);
  if (tmp)
    {
      _tgt.store(nullptr, cxx::memory_order_release);
      unblock_all(true);
      gate()->del(tmp);
      construct_gate<Ipc_gate_unbound>();
    }
}

Ipc_gate::Self_alloc *
Ipc_gate::allocator()
{ return _ipc_gate_allocator.slab(); }

Ipc_gate *
Ipc_gate::create(Ram_quota *q, Thread *t, Mword id)
{
  Auto_quota<Ram_quota> quota(q, sizeof(Ipc_gate));

  if (EXPECT_FALSE(!quota))
    return nullptr;

  void *nq = Ipc_gate::allocator()->alloc();
  if (EXPECT_FALSE(!nq))
    return nullptr;

  quota.release();
  return new (nq) Ipc_gate(q, t, id);
}

void Ipc_gate::operator delete (void *_f) noexcept
{
  Ipc_gate *f = cxx::launder(static_cast<Ipc_gate*>(_f));
  Ram_quota *p = f->_quota;

  allocator()->free(f);
  if (p)
    p->free(sizeof(Ipc_gate));
}


void
Ipc_gate::invoke(L4_obj_ref self, L4_fpage::Rights rights,
                     Syscall_frame *f, Utcb *utcb)
{
  if (f->tag().proto() == L4_msg_tag::Label_kobject
      && (f->ref().op() & L4_obj_ref::Ipc_send))
    Kobject_h<Ipc_gate, Kobject>::invoke(self, rights, f, utcb);
  else
    gate()->invoke(self, rights, f, utcb);
}

L4_msg_tag
Ipc_gate::kinvoke(L4_obj_ref self, L4_fpage::Rights rights,
                      Syscall_frame *f, Utcb const *in, Utcb *out)
{
  L4_msg_tag tag = f->tag();

  // Check for 'L4_msg_tag::Label_kobject' protocol in Ipc_gate_ctl::invoke().

  if (EXPECT_FALSE(tag.words() < 1))
    return commit_result(-L4_err::EInval);

  switch (in->values[0])
    {
    case Op_bind:
      return bind_thread(self, rights, f, in, out);
    case Op_get_info:
      return get_infos(self, rights, f, in, out);
    default:
      return kobject_invoke(self, rights, f, in, out);
    }
}



Ipc_gate::Ipc_gate(Ram_quota *q, Thread *t, Mword id)
: _quota(q)
{
  if (t)
    construct_gate<Ipc_gate_bound>(t, id);
  else
    construct_gate<Ipc_gate_unbound>();
}

namespace {
static Kobject_iface * FIASCO_FLATTEN
ipc_gate_factory(Ram_quota *q, Space *space,
                 L4_msg_tag tag, Utcb const *utcb,
                 int *err)
{
  L4_snd_item_iter snd_items(utcb, tag.words());
  Thread *thread = nullptr;
  Mword id = 0;

  if (tag.items() && snd_items.next())
    {
      L4_fpage bind_thread(snd_items.get()->d);
      *err = L4_err::EInval;
      if (EXPECT_FALSE(!bind_thread.is_objpage()))
        return nullptr;

      L4_msg_tag res;
      thread = space->lookup_local(bind_thread.obj_index(), L4_fpage::Rights::CS()).deref<Thread>(&res);

      if (EXPECT_FALSE(!thread))
        {
          *err = -res.proto();
          return nullptr;
        }

      if (EXPECT_FALSE(tag.words() < 3))
        {
          *err = L4_err::EMsgtooshort;
          return nullptr;
        }

      id = utcb->values[2];
    }

  *err = L4_err::ENomem;
  return Ipc_gate::create(q, thread, id);
}

static inline void __attribute__((constructor)) FIASCO_INIT_SFX(ipc_gate_register_factory)
register_factory()
{
  Kobject_iface::set_factory(0, ipc_gate_factory);
  Kobject_iface::set_factory(L4_msg_tag::Label_kobject, ipc_gate_factory);
}
}

#if defined (CONFIG_JDB)

#include "string_buffer.h"

void
Ipc_gate::Log_ipc_gate_invoke::print(String_buffer *buf) const
{
  buf->printf("D-gate=%lx D-thread=%lx L=%lx",
              gate_dbg_id, thread_dbg_id, label);
}

#endif // CONFIG_JDB

