#pragma once

#include <cxx/atomic>

#include "globalconfig.h"
#include "kobject.h"
#include "kobject_helper.h"
#include "slab_cache.h"
#include "assert_opt.h"

#if defined (CONFIG_JDB)
#include "tb_entry.h"
#endif // CONFIG_JDB

#include <cstddef>

class Ram_quota;
struct Inner_gate;

class Ipc_gate_if : public Kobject_iface
{
protected:
  Inner_gate *_this()
  { return reinterpret_cast<Inner_gate *>(this); }

  Inner_gate const *_this() const
  { return reinterpret_cast<Inner_gate const *>(this); }

public:
  virtual void del(Kobject_iface *) = 0;
  virtual void del_notify() = 0;
  virtual bool can_retype() const = 0;
};

struct Inner_gate
{
  Mword _gate_storage[(sizeof(Ipc_gate_if) + sizeof(Mword) - 1) / sizeof(Mword)];

  cxx::atomic<Thread *> _tgt;
  cxx::atomic<Mword> _id;
  Locked_prio_list _wait_q;

  template<typename T, typename ...ARGS>
  T *construct_gate(ARGS &&...args)
  {
    static_assert(sizeof(T) <= sizeof(_gate_storage));
    static_assert(alignof(T) <= alignof(_gate_storage));
    return new (&_gate_storage) T(cxx::forward<ARGS>(args)...);
  }

  static Inner_gate *from_gate(Ipc_gate_if *i)
  { return reinterpret_cast<Inner_gate *>(i); }

  static Inner_gate const *from_gate(Ipc_gate_if const *i)
  { return reinterpret_cast<Inner_gate const *>(i); }

  Ipc_gate_if *gate()
  { return reinterpret_cast<Ipc_gate_if *>(&_gate_storage); }

  Ipc_gate_if const *gate() const
  { return reinterpret_cast<Ipc_gate_if const *>(&_gate_storage); }

  Thread *target() const { return _tgt.load(cxx::memory_order_acquire); }
};


class Ipc_gate final :
  public cxx::Dyn_castable<Ipc_gate, Kobject_h<Ipc_gate, Kobject>>,
  private Inner_gate
{
public:
  typedef Slab_cache Self_alloc;
  static Self_alloc *allocator();
  static Ipc_gate *create(Ram_quota *q, Thread *t, Mword id);

  Kobject_iface *downgrade(unsigned long attr) override;

  void invoke(L4_obj_ref self, L4_fpage::Rights rights,
              Syscall_frame *f, Utcb *utcb) override;

  L4_msg_tag kinvoke(L4_obj_ref self, L4_fpage::Rights rights,
                     Syscall_frame *f, Utcb const *in, Utcb *out);

  static Ipc_gate *from_gate(Ipc_gate_if *i)
  { return static_cast<Ipc_gate *>(Inner_gate::from_gate(i)); }

  static Ipc_gate const *from_gate(Ipc_gate_if const *i)
  { return static_cast<Ipc_gate const *>(Inner_gate::from_gate(i)); }

private:
  friend class Jdb_sender_list;

  friend Ipc_gate *ipc_gate_obj(void *g);
  friend Ipc_gate const *ipc_gate_obj(void const *g);

  enum Operation
  {
    Op_bind     = 0x10,
    Op_get_info = 0x11,
  };

  L4_msg_tag bind_thread(L4_obj_ref, L4_fpage::Rights rights,
                         Syscall_frame *f, Utcb const *in, Utcb *);

  L4_msg_tag get_infos(L4_obj_ref, L4_fpage::Rights,
                       Syscall_frame *, Utcb const *, Utcb *out);

  Ram_quota *_quota;

public:
  Ipc_gate(Ram_quota *q, Thread *t, Mword id);

  Thread *target() const { return _tgt.load(cxx::memory_order_relaxed); }
  Mword id() const { return _id.load(cxx::memory_order_relaxed); }
  Mword obj_id() const override { return id(); }
  bool is_local(Space *s) const override { return gate()->is_local(s); }

  //::Kobject_mappable *map_root() override
  //{ return Kobject::map_root(); }

  void unblock_all(bool abort = false);
  void destroy(Kobject ***r) override;

  ~Ipc_gate() noexcept
  {
    unblock_all(true);
  }

  void *operator new (size_t, void *b) noexcept
  { return b; }

  void operator delete (void *_f) noexcept;

#if defined (CONFIG_JDB)
  ::Kobject_dbg *dbg_info() const override
  { return Kobject::dbg_info(); }

  struct Log_ipc_gate_invoke : public Tb_entry
  {
    Mword gate_dbg_id;
    Mword thread_dbg_id;
    Mword label;
    void print(String_buffer *buf) const;
  };
#endif // CONFIG_JDB
};

