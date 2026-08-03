
#include "ipc_gate.h"
#include "globalconfig.h"
#include "ipc_timeout.h"
#include "kmem_slab.h"
#include "kobject_rpc.h"
#include "static_init.h"
#include <system_clock.h>

JDB_DEFINE_TYPENAME(Ipc_gate_obj, "\033[35mGate\033[m");
static Kmem_slab_t<Ipc_gate_obj> _ipc_gate_allocator("Ipc_gate");

using Ipc_flags = Thread::Ipc_flags;

Kobject_iface *
Ipc_gate_obj::downgrade(unsigned long attr)
{
  if (attr & L4_msg_item::C_obj_right_1)
    return poly();
  else
    return this;
}

inline
L4_msg_tag
Ipc_gate_obj::bind_thread(L4_obj_ref, L4_fpage::Rights rights,
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

  Poly_ipc_gate x;
  x.construct<Ipc_gate>();
  t->inc_ref();

  _id.store(in->values[1]);
  auto *old = _tgt.load(cxx::memory_order_relaxed);
  while (!_tgt.compare_exchange_strong(old, t))
    ;

  if (!old)
    poly() = x;

  unblock_all();
  current()->rcu_wait();
  unblock_all();

  if (old)
    poly()->del(old);

  return commit_result(0);
}

inline
L4_msg_tag
Ipc_gate_obj::get_infos(L4_obj_ref, L4_fpage::Rights,
                        Syscall_frame *, Utcb const *, Utcb *out)
  {
    out->values[0] = id();
    return commit_result(0, 1);
  }


void
Ipc_gate_obj::unblock_all(bool abort)
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
Ipc_gate_obj::destroy(Kobject ***r)
{
  Kobject::destroy(r);
  poly()->del_notify();
  auto *tmp = _tgt.load(cxx::memory_order_acquire);
  if (tmp)
    {
      _tgt.store(nullptr, cxx::memory_order_release);
      unblock_all(true);
      poly()->del(tmp);
      poly().construct<Ipc_gate_unbound>();
    }
}

Ipc_gate_obj::Self_alloc *
Ipc_gate_obj::allocator()
{ return _ipc_gate_allocator.slab(); }

Ipc_gate_obj *
Ipc_gate::create(Ram_quota *q, Thread *t, Mword id)
{
  Auto_quota<Ram_quota> quota(q, sizeof(Ipc_gate_obj));

  if (EXPECT_FALSE(!quota))
    return nullptr;

  void *nq = Ipc_gate_obj::allocator()->alloc();
  if (EXPECT_FALSE(!nq))
    return nullptr;

  quota.release();
  return new (nq) Ipc_gate_obj(q, t, id);
}

void Ipc_gate_obj::operator delete (void *_f) noexcept
{
  Ipc_gate_obj *f = cxx::launder(static_cast<Ipc_gate_obj*>(_f));
  Ram_quota *p = f->_quota;

  allocator()->free(f);
  if (p)
    p->free(sizeof(Ipc_gate_obj));
}


void
Ipc_gate_obj::invoke(L4_obj_ref self, L4_fpage::Rights rights,
                     Syscall_frame *f, Utcb *utcb)
{
  if (f->tag().proto() == L4_msg_tag::Label_kobject
      && (f->ref().op() & L4_obj_ref::Ipc_send))
    Kobject_h<Ipc_gate_obj, Kobject>::invoke(self, rights, f, utcb);
  else
    poly()->invoke(self, rights, f, utcb);
}

L4_msg_tag
Ipc_gate_obj::kinvoke(L4_obj_ref self, L4_fpage::Rights rights,
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


void
Ipc_gate_unbound::initiate_deletion(Kobject ***rl)
{ Ipc_gate_obj::from_poly(this)->initiate_deletion(rl); }

Kobject_mappable *
Ipc_gate_unbound::map_root()
{ return Ipc_gate_obj::from_poly(this)->map_root(); }

inline
L4_error
Ipc_gate_unbound::block(Thread *ct, L4_timeout const &to, Utcb *u)
{
  Unsigned64 tval = 0;
  if (!to.is_never())
    {
      Unsigned64 system_clock = System_clock::clock();
      tval = to.microsecs(system_clock, u);
      if (tval == 0 || tval <= system_clock)
        return L4_error::Timeout;
    }

  ct->sender_enqueue(&Ipc_gate_obj::from_poly(this)->_wait_q, ct->sched_context()->prio());
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

  ct->sender_dequeue(&Ipc_gate_obj::from_poly(this)->_wait_q);

  if (state & Thread_timeout)
    return L4_error::Timeout;

  if (state & Thread_cancel)
    return L4_error::Canceled;

  return L4_error::None;
}

void
Ipc_gate_unbound::invoke(L4_obj_ref self, L4_fpage::Rights rights,
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


Ipc_gate::Ipc_gate(Thread *t, Mword id)
{
  t->inc_ref();
  Ipc_gate_obj::from_poly(this)->_id.store(id);
  Ipc_gate_obj::from_poly(this)->_tgt.store(t);
}

bool
Ipc_gate::is_local(Space *s) const
{
  return Ipc_gate_obj::target_thread(this)->space() == s;
}

Mword
Ipc_gate::obj_id() const
{ return Ipc_gate_obj::from_poly(this)->obj_id(); }

void
Ipc_gate::initiate_deletion(Kobject ***rl)
{ Ipc_gate_obj::from_poly(this)->initiate_deletion(rl); }

Kobject_mappable *
Ipc_gate::map_root()
{ return Ipc_gate_obj::from_poly(this)->map_root(); }

Kobject_iface *
Ipc_gate::downgrade(long unsigned int)
{ return this; }

void
Ipc_gate::del(Kobject_iface *o)
{
  auto t = nonull_static_cast<Thread *>(o);
  if (t->dec_ref() == 0)
    delete t;
}

void
Ipc_gate::del_notify()
{
  Ipc_gate_obj::target_thread(this)->ipc_gate_deleted(Ipc_gate_obj::from_poly(this)->id());
}

FIASCO_FLATTEN
void
Ipc_gate::invoke(L4_obj_ref, L4_fpage::Rights rights,
                 Syscall_frame *f, Utcb *utcb)
{
  //LOG_MSG_3VAL(current(), "gIPC", Mword(_thread), _id, f->obj_2_flags());
  //printf("Invoke: Ipc_gate(%lx->%p)...\n", _id, _thread);
  Thread *ct = current_thread();
  Thread *sender = nullptr;
  Thread *partner = nullptr;
  bool have_rcv = false;

  Thread *t = Ipc_gate_obj::target_thread(this);
  if (EXPECT_FALSE(!t))
    {
      f->tag(commit_error(utcb, L4_error::Not_existent));
      return;
    }

  bool ipc = t->check_sys_ipc(f->ref().op(), &partner, &sender, &have_rcv);

  LOG_TRACE("IPC Gate invoke", "gate", current(), Ipc_gate_obj::Log_ipc_gate_invoke,
      l->gate_dbg_id = Ipc_gate_obj::from_poly(this)->dbg_id();
      l->thread_dbg_id = t->dbg_id();
      l->label = Ipc_gate_obj::from_poly(this)->_id.load(cxx::memory_order_relaxed) | cxx::int_value<L4_fpage::Rights>(rights);
  );

  if (EXPECT_FALSE(!ipc))
    f->tag(commit_error(utcb, L4_error::Not_existent));
  else
    {
      ct->set_ipc_from_spec(Ipc_gate_obj::from_poly(this)->_id.load()
                            | cxx::int_value<L4_fpage::Rights>(rights), rights, partner);
      ct->do_ipc(f->tag(), partner, Ipc_flags(have_rcv, !sender), sender, f->timeout(), f);
    }
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

Kobject_dbg *
Ipc_gate::dbg_info() const
{ return Ipc_gate_obj::from_poly(this)->dbg_info(); }

Kobject_dbg *
Ipc_gate_unbound::dbg_info() const
{ return Ipc_gate_obj::from_poly(this)->dbg_info(); }

void
Ipc_gate_obj::Log_ipc_gate_invoke::print(String_buffer *buf) const
{
  buf->printf("D-gate=%lx D-thread=%lx L=%lx",
              gate_dbg_id, thread_dbg_id, label);
}

#endif // CONFIG_JDB

