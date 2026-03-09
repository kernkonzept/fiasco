INTERFACE:

#include "atomic.h"
#include "l4_types.h"

class Receiver;

/// Helper class to encode and decode value stored in reply cap slot.
class Reply_cap
{
  friend class Reply_cap_slot;

public:
  constexpr Reply_cap() : raw(0) {}

  Reply_cap(Receiver *caller, L4_fpage::Rights rights)
  : raw(reinterpret_cast<Mword>(caller)
        | (cxx::int_value<L4_fpage::Rights>(rights) & 0x3UL))
  {}

  bool constexpr is_valid() const noexcept { return raw != 0; }
  explicit constexpr operator bool() const { return is_valid(); }

  Receiver *caller() const
  { return reinterpret_cast<Receiver*>(raw & ~0x03UL); }

  constexpr L4_fpage::Rights caller_rights() const
  { return L4_fpage::Rights(raw & 0x03UL); }

  static constexpr Reply_cap Empty() { return Reply_cap(); }

private:
  Mword raw;
};

/// A reply cap slot. Used for implicit and explicit reply cap slots.
class Reply_cap_slot
{
public:
  Reply_cap_slot() = default;

  // Must not be copied or moved because we hold pointers to the slots!
  Reply_cap_slot(Reply_cap_slot const &) = delete;
  Reply_cap_slot &operator=(Reply_cap_slot const &) = delete;

  /// Can be used heuristically.
  bool is_used() const { return _slot.is_valid(); }

  bool cas(Reply_cap oldval, Reply_cap newval)
  {
    return ::cas(&_slot.raw, oldval.raw, newval.raw);
  }

  Reply_cap read_dirty() const { return _slot; }
  Reply_cap read_atomic() const { return atomic_load(&_slot); }

  void write_atomic(Reply_cap newval)
  { atomic_store(&_slot, newval); }

  /// Atomically replace Reply_cap with an empty one.
  Reply_cap reset_atomic()
  { return atomic_exchange(&_slot, Reply_cap::Empty()); }

private:
  Reply_cap _slot;
};

//---------------------------------------------------------------------------
INTERFACE[obj_space_phys_avl]:

#include "cxx/avl_map"
#include "global_data.h"
#include "kmem_slab.h"

template< typename _Type >
class Reply_space_allocator
{
public:
  using Type = _Type;
  using Allocator = Kmem_slab_t<Type>;

private:
  static Global_data<Allocator> _reply_cap_alloc;

  Ram_quota *_q;

public:
  enum { can_free = true };

  Reply_space_allocator(Ram_quota *q) noexcept : _q(q) {}
  Reply_space_allocator(Reply_space_allocator const &other) noexcept
  : _q(other._q)
  {}

  _Type *alloc() noexcept
  { return static_cast<_Type*>(_reply_cap_alloc->q_alloc(_q)); }

  void free(_Type *t) noexcept
  { _reply_cap_alloc->q_free(_q, t); }
};

struct Reply_space_node
{
  Reply_space_node() = default;
  Reply_space_node(Reply_space_node const &) = delete;
  Reply_space_node &operator=(Reply_space_node const &) = delete;

  static constexpr unsigned Size = 8;
  Reply_cap_slot reply_caps[Size];
};

using Reply_space_map = cxx::Avl_map<Mword, Reply_space_node, cxx::Lt_functor,
                                     Reply_space_allocator>;

//---------------------------------------------------------------------------
INTERFACE:

#include "config.h"
#include "kmem_alloc.h"
#include "l4_types.h"
#include "ram_quota.h"

template<typename SPACE>
class Reply_space
{
public:
  static constexpr Mword Empty_reply_cap = 0UL;

  Reply_cap_slot const *get_reply_cap(Reply_cap_index index) const;
  Reply_cap_slot *access_reply_cap(Reply_cap_index index, bool alloc);

  /**
   * Clear reply space.
   *
   * Iterates the whole reply space and invokes the `dispose` callback for each
   * populated reply cap slot.
   *
   * \attention This is a preemption point! Interrupts will be opened
   *            during iteration.
   *
   * \param dispose   Callback that clears the passed reply cap slot.
   */
  template<typename DISPOSE>
  void clear(DISPOSE dispose);

private:
  Ram_quota *ram_quota() const
  {
    return static_cast<SPACE const *>(this)->ram_quota();
  }

  Mword _bound_threads = 0;
};

//---------------------------------------------------------------------------
IMPLEMENTATION:

/**
 * Add bound thread.
 */
PUBLIC template<typename SPACE> inline
void
Reply_space<SPACE>::add_bound_thread()
{ atomic_add(&_bound_threads, 1); }

/**
 * Remove bound thread.
 *
 * This is a preemption point! The caller is supposed to hold a counted
 * reference!
 *
 * The `dispose` callback is invoked for every populated reply cap slot if this
 * was the last bound thread. Theoretically, user space could concurrently bind
 * a new thread and the operation could race with new reply cap allocations.
 * This is deemed very unlikely because the end of the last thread in a task is
 * normally the end of the task too.
 */
PUBLIC template<typename SPACE> template<typename DISPOSE> inline
void
Reply_space<SPACE>::remove_bound_thread(DISPOSE dispose)
{
  // Make sure updates to reply cap slots are visible before the counter is
  // updated. Pairs with Mem::mp_rmb() below.
  Mem::mp_wmb();

  if (atomic_add_fetch(&_bound_threads, -1) == 0)
    {
      // Make sure we read the reply space only _after_ the counter was
      // updated. Pairs with the Mem::mp_wmb() above. Prevents speculative
      // reads of reply cap slots.
      Mem::mp_rmb();
      clear<DISPOSE>(dispose);
    }
}

//---------------------------------------------------------------------------
INTERFACE[!obj_space_phys_avl]:

EXTENSION class Reply_space
{
  // Implement the reply cap space as a two-stage table.
  struct L1_table
  {
    static constexpr Bytes Size = Config::page_size();
    static constexpr size_t Slots = Config::PAGE_SIZE / sizeof(Reply_cap_slot);

    Reply_cap_slot reply_caps[Slots];

    static L1_table *alloc(Ram_quota *q)
    {
      L1_table *t = static_cast<L1_table *>(
        Kmem_alloc::allocator()->q_alloc(q, Size));
      if (!t)
        return nullptr;

      new (t) L1_table();
      return t;
    }

    void free(Ram_quota *q)
    { Kmem_alloc::allocator()->q_free(q, Size, this); }

  private:
    L1_table() = default;
  };

  static_assert(sizeof(L1_table) == Config::PAGE_SIZE);

  struct L0_table
  {
    static constexpr Bytes Size = Config::page_size();
    static constexpr size_t Slots = Config::PAGE_SIZE / sizeof(L1_table *);

    L1_table *dir[Slots] = { nullptr, };

    static L0_table *alloc(Ram_quota *q)
    {
      L0_table *t = static_cast<L0_table *>(
        Kmem_alloc::allocator()->q_alloc(q, Size));
      if (!t)
        return nullptr;

      new (t) L0_table();
      return t;
    }

    void free(Ram_quota *q)
    { Kmem_alloc::allocator()->q_free(q, Size, this); }

  private:
    L0_table() = default;
  };

  static_assert(sizeof(L0_table) == Config::PAGE_SIZE);

public:
  static constexpr auto Num_reply_caps = Reply_cap_index(  L0_table::Slots
                                                         * L1_table::Slots);

private:
  static Mword l0_index(Reply_cap_index idx)
  { return cxx::int_value<Reply_cap_index>(idx) / L1_table::Slots; }

  static Mword l1_index(Reply_cap_index idx)
  { return cxx::int_value<Reply_cap_index>(idx) % L1_table::Slots; }

  // Must not be deallocated / moved after once allocated, otherwise pointers
  // to reply cap slots, e.g. stored in Receiver::_partner_reply_cap would
  // become invalid. Must only be freed in the final destruction phase of
  // Space, i.e. in its destructor.
  L0_table *_reply_caps = nullptr;
};

//---------------------------------------------------------------------------
IMPLEMENTATION[!obj_space_phys_avl]:

PUBLIC template<typename SPACE>
Reply_space<SPACE>::~Reply_space()
{
  if (!_reply_caps)
    return;

  for (L1_table *l1 : _reply_caps->dir)
    if (l1)
      {
        // Ensure that all reply caps were freed, so no dangling reply caps
        // in receiver.
        for (auto &cap [[maybe_unused]] : l1->reply_caps)
          assert(!cap.is_used());

        l1->free(ram_quota());
      }

  _reply_caps->free(ram_quota());
}

IMPLEMENT template<typename SPACE>
Reply_cap_slot const *
Reply_space<SPACE>::get_reply_cap(Reply_cap_index index) const
{
  if (!_reply_caps || index >= Num_reply_caps) [[unlikely]]
    return nullptr;

  L1_table const *l1 = _reply_caps->dir[l0_index(index)];
  if (!l1) [[unlikely]]
    return nullptr;

  return &l1->reply_caps[l1_index(index)];
}

IMPLEMENT template<typename SPACE>
Reply_cap_slot *
Reply_space<SPACE>::access_reply_cap(Reply_cap_index index, bool alloc)
{
  if (index >= Num_reply_caps)
    return nullptr;

  L0_table *l0 = _reply_caps;
  if (!l0) [[unlikely]]
    {
      if (!alloc)
        return nullptr;

      l0 = L0_table::alloc(ram_quota());
      if (!l0)
        return nullptr;

      // Page clearing must be observable *before* the pointer to the table is
      // visible! No Mem::mp_rmb() needed in get_reply_cap() because of the
      // data dependency.
      Mem::mp_wmb();

      if (!cas<L0_table *>(&_reply_caps, nullptr, l0))
        {
          // Some other thread was faster.
          l0->free(ram_quota());
          l0 = _reply_caps;
        }
    }

  L1_table *l1 = l0->dir[l0_index(index)];
  if (!l1) [[unlikely]]
    {
      if (!alloc)
        return nullptr;

      l1 = L1_table::alloc(ram_quota());
      if (!l1)
        return nullptr;

      // Object initialization must be observable *before* the pointer to the
      // table is visible! No Mem::mp_rmb() needed in get_reply_cap() because
      // of the data dependency.
      Mem::mp_wmb();

      if (!cas<L1_table *>(&l0->dir[l0_index(index)], nullptr, l1))
        {
          // Some other thread was faster.
          l1->free(ram_quota());
          l1 = l0->dir[l0_index(index)];
        }
    }

  return &l1->reply_caps[l1_index(index)];
}

IMPLEMENT template<typename SPACE> template<typename DISPOSE>
void
Reply_space<SPACE>::clear(DISPOSE dispose)
{
  if (!_reply_caps)
    return;

  // Open IRQs while sweeping through the reply cap space. The caller is
  // supposed to hold a counted reference to us!
  auto guard = lock_guard<Lock_guard_inverse_policy>(cpu_lock);

  for (L1_table *l1 : _reply_caps->dir)
    if (l1)
      for (auto &cap : l1->reply_caps)
        if (cap.is_used())
          dispose(&cap);
}

//---------------------------------------------------------------------------
INTERFACE[obj_space_phys_avl]:

#include "spin_lock.h"

EXTENSION class Reply_space
{
  // Implement the reply cap space as AVL tree. Each node holds 8 reply caps.

  static Mword map_index(Reply_cap_index idx)
  { return cxx::int_value<Reply_cap_index>(idx) / Reply_space_node::Size; }

  static Mword node_index(Reply_cap_index idx)
  { return cxx::int_value<Reply_cap_index>(idx) % Reply_space_node::Size; }

  Reply_space_map _reply_caps;
  mutable Spin_lock<> _lock;
};

//---------------------------------------------------------------------------
IMPLEMENTATION[obj_space_phys_avl]:

template<>
DEFINE_GLOBAL
Global_data<Reply_space_map::Node_allocator::Allocator>
Reply_space_allocator<Reply_space_map::Node_allocator::Type>::_reply_cap_alloc("Reply_caps");


PUBLIC template<typename SPACE>
Reply_space<SPACE>::Reply_space()
: _reply_caps(Reply_space_map::Node_allocator(ram_quota()))
{}

PUBLIC template<typename SPACE>
Reply_space<SPACE>::~Reply_space()
{
  // Ensure that all reply caps were freed, so no dangling reply caps in
  // receiver.
  for (auto &n : _reply_caps)
    for (auto &cap [[maybe_unused]] : n.second.reply_caps)
      assert(!cap.is_used());
}

IMPLEMENT template<typename SPACE>
Reply_cap_slot const *
Reply_space<SPACE>::get_reply_cap(Reply_cap_index index) const
{
  auto guard = lock_guard(_lock);

  auto e = _reply_caps.find_node(map_index(index));
  if (!e) [[unlikely]]
    return nullptr;

  return &e->second.reply_caps[node_index(index)];
}

IMPLEMENT template<typename SPACE>
Reply_cap_slot *
Reply_space<SPACE>::access_reply_cap(Reply_cap_index index, bool alloc)
{
  auto guard = lock_guard(_lock);

  auto e = _reply_caps.find_node(map_index(index));
  if (e)
    return const_cast<Reply_cap_slot *>(&e->second.reply_caps[node_index(index)]);

  if (!alloc)
    return nullptr;

  auto n = _reply_caps.emplace(map_index(index));
  if (n.second == -Reply_space_map::E_nomem)
    return nullptr;

  return &n.first->second.reply_caps[node_index(index)];
}

IMPLEMENT template<typename SPACE> template<typename DISPOSE>
void
Reply_space<SPACE>::clear(DISPOSE dispose)
{
  Reply_space_map old{Reply_space_map::Node_allocator(ram_quota())};

  // We cannot hold the lock while iterating the map. Instead, move tree into
  // local variable so that we know it cannot be modified.
  {
    auto guard = lock_guard(_lock);
    old = cxx::move(_reply_caps);
  }

  // Open IRQs while sweeping through the reply cap space. The caller is
  // supposed to hold a counted reference to us!
  auto guard = lock_guard<Lock_guard_inverse_policy>(cpu_lock);

  for (auto &n : _reply_caps)
    for (auto &cap : n.second.reply_caps)
      if (cap.is_used())
        dispose(&cap);

  // Destroy map while interrupts are open. The data structure could be large!
  old.clear();
}
