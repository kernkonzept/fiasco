INTERFACE:

#include "kobject.h"
#include "kobject_helper.h"
#include "slab_cache.h"
#include "thread_object.h"

class Ram_quota;

class Ipc_gate_obj;

class Ipc_gate_ctl : public Kobject_h<Ipc_gate_ctl, Kobject_iface>
{
private:
  enum class Op : Mword
  {
    Bind     = 0x10,
    Get_info = 0x11,
  };
};

class Ipc_gate : public cxx::Dyn_castable<Ipc_gate, Kobject>
{
  friend class Ipc_gate_ctl;
  friend class Jdb_sender_list;
protected:

  Thread *_thread;
  Mword _id;
  Ram_quota *_quota;
  Locked_prio_list _wait_q;
};

class Ipc_gate_obj :
  public cxx::Dyn_castable<Ipc_gate_obj, Ipc_gate, Ipc_gate_ctl>
{
  friend class Ipc_gate;

public:
  bool put() override { return Ipc_gate::put(); }

  Thread *thread() const { return _thread; }
  Mword id() const { return _id; }
  Mword obj_id() const override { return _id; }

  bool is_local(Space *s) const override
  {
    Thread *t = access_once(&_thread);
    return t && t->space() == s;
  }
};

//---------------------------------------------------------------------------
INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Ipc_gate
{
protected:
  struct Log_ipc_gate_invoke : public Tb_entry
  {
    Mword gate_dbg_id;
    Mword thread_dbg_id;
    Mword label;
    void print(String_buffer *buf) const;
  };
  static_assert(sizeof(Log_ipc_gate_invoke) <= Tb_entry::Tb_entry_size);
};

//---------------------------------------------------------------------------
IMPLEMENTATION:

#include <cstddef>

#include "entry_frame.h"
#include "ipc_timeout.h"
#include "kmem_slab.h"
#include "kobject_rpc.h"
#include "logdefs.h"
#include "ram_quota.h"
#include "static_init.h"
#include "thread.h"
#include "thread_state.h"
#include "timer.h"
#include "global_data.h"

JDB_DEFINE_TYPENAME(Ipc_gate_obj, "\033[35mGate\033[m");

PUBLIC
::Kobject_mappable *
Ipc_gate_obj::map_root() override
{ return Ipc_gate::map_root(); }

PUBLIC
Kobject_iface *
Ipc_gate_ctl::downgrade(unsigned long attr) override
{
  if (attr & L4_snd_item::C_obj_right_1)
    return static_cast<Ipc_gate*>(static_cast<Ipc_gate_obj*>(this));
  else
    return this;
}

PUBLIC inline
Ipc_gate::Ipc_gate(Ram_quota *q, Thread *t, Mword id)
  : _thread(nullptr), _id(id), _quota(q), _wait_q()
{
  if (t)
    {
      t->inc_ref();
      _thread = t;
    }
}

PUBLIC inline
Ipc_gate_obj::Ipc_gate_obj(Ram_quota *q, Thread *t, Mword id)
  : Dyn_castable_class(q, t, id)
{}

PUBLIC
void
Ipc_gate_obj::unblock_all()
{
  while (::Prio_list_elem *h = _wait_q.first())
    {
      auto g1 = lock_guard(cpu_lock);
      Thread *w;
	{
	  auto g2 = lock_guard(_wait_q.lock());
	  if (EXPECT_FALSE(h != _wait_q.first()))
	    continue;

	  w = static_cast<Thread*>(Sender::cast(h));
	  w->sender_dequeue(&_wait_q);
	}
      w->activate();
    }
}

PUBLIC virtual
void
Ipc_gate_obj::initiate_deletion(Kobjects_list &reap_list) override
{
  if (_thread)
    _thread->ipc_gate_deleted(_id);

  Kobject::initiate_deletion(reap_list);
}

PUBLIC virtual
void
Ipc_gate_obj::destroy(Kobjects_list &reap_list) override
{
  Kobject::destroy(reap_list);

  Thread *tmp = access_once(&_thread);
  if (tmp)
    {
      _thread = nullptr;
      unblock_all();
      if (tmp->dec_ref() == 0)
        delete tmp;
    }
}

PUBLIC
Ipc_gate_obj::~Ipc_gate_obj()
{
  unblock_all();
}

PUBLIC inline NEEDS[<cstddef>]
void *
Ipc_gate_obj::operator new (size_t, void *b) noexcept
{ return b; }

static DEFINE_GLOBAL
Global_data<Kmem_slab_t<Ipc_gate_obj>> _ipc_gate_allocator("Ipc_gate");

PRIVATE static
void *
Ipc_gate_obj::alloc()
{ return _ipc_gate_allocator->alloc(); }

PRIVATE static
void
Ipc_gate_obj::free(void *f)
{ _ipc_gate_allocator->free(f); }

PUBLIC static
Ipc_gate_obj *
Ipc_gate::create(Ram_quota *q, Thread *t, Mword id)
{
  Auto_quota<Ram_quota> quota(q, sizeof(Ipc_gate_obj));

  if (EXPECT_FALSE(!quota))
    return nullptr;

  void *nq = Ipc_gate_obj::alloc();
  if (EXPECT_FALSE(!nq))
    return nullptr;

  quota.release();
  return new (nq) Ipc_gate_obj(q, t, id);
}

PUBLIC
void Ipc_gate_obj::operator delete (Ipc_gate_obj *o, std::destroying_delete_t)
{
  Ram_quota *q = o->_quota;

  o->~Ipc_gate_obj();

  free(o);
  if (q)
    q->free(sizeof(Ipc_gate_obj));
}

PRIVATE inline NOEXPORT
L4_msg_tag
Ipc_gate_ctl::bind_thread(L4_obj_ref, L4_fpage::Rights rights,
                          Syscall_frame *f, Utcb const *in, Utcb *)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  L4_msg_tag tag = f->tag();

  if (tag.words() < 2)
    return commit_result(-L4_err::EMsgtooshort);

  Mword id = access_once(&in->values[1]);
  if (EXPECT_FALSE(id & cxx::int_value<L4_fpage::Rights>(L4_fpage::Rights::CWS())))
    return commit_result(-L4_err::EInval);

  L4_fpage::Rights t_rights(0);
  Thread *t = Ko::deref<Thread>(&tag, in, &t_rights);
  if (!t)
    return tag;

  if (!(t_rights & L4_fpage::Rights::CS()))
    return commit_result(-L4_err::EPerm);

  Ipc_gate_obj *g = static_cast<Ipc_gate_obj*>(this);
  g->_id = id;
  t->inc_ref();

  Mem::mp_wmb(); // Ensure visibility of _id before _thread

  Thread *old;
  do
    {
      old = access_once(&g->_thread);
    }
  while (!cas(&g->_thread, old, t));

  g->unblock_all();
  current()->rcu_wait();
  g->unblock_all();

  if (old && old->dec_ref() == 0)
    delete old;

  return commit_result(0);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Ipc_gate_ctl::get_infos(L4_obj_ref, L4_fpage::Rights,
                        Syscall_frame *, Utcb const *, Utcb *out)
{
  Ipc_gate_obj *g = static_cast<Ipc_gate_obj*>(this);
  out->values[0] = g->_id;
  return commit_result(0, 1);
}

PUBLIC
void
Ipc_gate_ctl::invoke(L4_obj_ref self, L4_fpage::Rights rights,
                     Syscall_frame *f, Utcb *utcb) override
{
  if (f->tag().proto() == L4_msg_tag::Label_kobject
      && (f->ref().op() & L4_obj_ref::Ipc_send))
    Kobject_h<Ipc_gate_ctl, Kobject_iface>::invoke(self, rights, f, utcb);
  else
    static_cast<Ipc_gate_obj*>(this)->Ipc_gate::invoke(self, rights, f, utcb);
}


PUBLIC
L4_msg_tag
Ipc_gate_ctl::kinvoke(L4_obj_ref self, L4_fpage::Rights rights,
                      Syscall_frame *f, Utcb const *in, Utcb *out)
{
  L4_msg_tag tag = f->tag();

  // Check for 'L4_msg_tag::Label_kobject' protocol in Ipc_gate_ctl::invoke().

  if (EXPECT_FALSE(tag.words() < 1))
    return commit_result(-L4_err::EInval);

  switch (Op{in->values[0]})
    {
    case Op::Bind:
      return bind_thread(self, rights, f, in, out);
    case Op::Get_info:
      return get_infos(self, rights, f, in, out);
    default:
      return static_cast<Ipc_gate_obj*>(this)->kobject_invoke(self, rights, f, in, out);
    }
}

PRIVATE inline NOEXPORT
L4_error
Ipc_gate::block(Thread *ct, L4_timeout const &to, Utcb *u)
{
  Unsigned64 tval = 0;
  if (!to.is_never())
    {
      Unsigned64 clock = Timer::system_clock();
      tval = to.microsecs(clock, u);
      if (tval == 0 || tval <= clock)
        return L4_error::Timeout;
    }

    {
      auto g = lock_guard(_wait_q.lock());
      ct->set_wait_queue(&_wait_q);
      ct->sender_enqueue(&_wait_q, ct->sched_context()->prio());
    }
  ct->state_change_dirty(~Thread_ready, Thread_send_wait);

  IPC_timeout timeout;
  if (tval)
    {
      timeout.set(tval, current_cpu());
      ct->set_timeout(&timeout);
    }
  // else infinite timeout

  ct->schedule();

  Mword state = ct->state_change(~Thread_full_ipc_mask, Thread_ready);
  ct->reset_timeout();

  if (EXPECT_FALSE(ct->in_sender_list()))
    {
      auto g = lock_guard(_wait_q.lock());
      // Recheck under lock whether thread is still in waiting queue.
      if (ct->in_sender_list())
        {
          ct->sender_dequeue(&_wait_q);

          if (state & Thread_timeout)
            return L4_error::Timeout;

          if (state & Thread_cancel)
            return L4_error::Canceled;
        }
    }

  return L4_error::None;
}


PUBLIC
void
Ipc_gate::invoke(L4_obj_ref /*self*/, L4_fpage::Rights rights,
                 Syscall_frame *f, Utcb *utcb) override
{
  Thread *ct = current_thread();
  Thread *sender = nullptr;
  Thread *partner = nullptr;
  bool have_rcv = false;

  Thread *t = access_once(&_thread);
  if (EXPECT_FALSE(!t))
    {
      L4_error e = block(ct, f->timeout().snd, utcb);
      if (!e.ok())
	{
	  f->tag(commit_error(utcb, e));
	  return;
	}

      t = access_once(&_thread);
      if (EXPECT_FALSE(!t))
	{
	  f->tag(commit_error(utcb, L4_error::Not_existent));
	  return;
	}
    }

  bool ipc = t->check_sys_ipc(f->ref().op(), &partner, &sender, &have_rcv);

  Mem::mp_rmb(); // Ensure that _id is initialized.

  LOG_TRACE("IPC Gate invoke", "gate", current(), Log_ipc_gate_invoke,
      l->gate_dbg_id = dbg_id();
      l->thread_dbg_id = t->dbg_id();
      l->label = _id | cxx::int_value<L4_fpage::Rights>(rights);
  );

  if (EXPECT_FALSE(!ipc))
    f->tag(commit_error(utcb, L4_error::Not_existent));
  else
    {
      Mword const from_spec = _id | cxx::int_value<L4_fpage::Rights>(rights);
      ct->do_ipc(f->tag(), from_spec, partner, have_rcv, sender, f->timeout(), f, rights);
    }
}

namespace {

static Kobject_iface * FIASCO_FLATTEN
ipc_gate_factory(Ram_quota *q, Space *space,
                 L4_msg_tag tag, Utcb const *utcb, Utcb *,
                 int *err, unsigned *)
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

      L4_fpage::Rights thread_rights = L4_fpage::Rights(0);
      thread = cxx::dyn_cast<Thread*>(space->lookup_local(bind_thread.obj_index(),
                                                          &thread_rights));

      if (EXPECT_FALSE(!thread))
        {
          *err = L4_err::EInval;
          return nullptr;
        }

      if (EXPECT_FALSE(!(thread_rights & L4_fpage::Rights::CS())))
        {
          *err = L4_err::EPerm;
          return nullptr;
        }

      if (EXPECT_FALSE(tag.words() < 3))
        {
          *err = L4_err::EMsgtooshort;
          return nullptr;
        }

      id = access_once(&utcb->values[2]);
      if (EXPECT_FALSE(id & cxx::int_value<L4_fpage::Rights>(L4_fpage::Rights::CWS())))
        {
          *err = L4_err::EInval;
          return nullptr;
        }
    }

  *err = L4_err::ENomem;
  return static_cast<Ipc_gate_ctl*>(Ipc_gate::create(q, thread, id));
}

static inline
void __attribute__((constructor)) FIASCO_INIT_SFX(ipc_gate_register_factory)
register_factory()
{
  Kobject_iface::set_factory(0, ipc_gate_factory);
  Kobject_iface::set_factory(L4_msg_tag::Label_kobject, ipc_gate_factory);
}

}

//---------------------------------------------------------------------------
IMPLEMENTATION [rt_dbg]:

PUBLIC
::Kobject_dbg *
Ipc_gate_obj::dbg_info() const override
{ return Ipc_gate::dbg_info(); }

//---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "string_buffer.h"

IMPLEMENT
void
Ipc_gate::Log_ipc_gate_invoke::print(String_buffer *buf) const
{
  buf->printf("D-gate=%lx D-thread=%lx L=%lx",
              gate_dbg_id, thread_dbg_id, label);
}

