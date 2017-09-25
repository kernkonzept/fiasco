INTERFACE:

#include "spin_lock.h"
#include "queue_item.h"
#include "assert.h"
#include <cxx/dlist>

class Queue
{
public:
  typedef Spin_lock_coloc<Mword> Inner_lock;

private:
  class Lock_n_ptr : public Inner_lock
  {
  public:
    Queue_item *item() const
    { return reinterpret_cast<Queue_item*>(get_unused() & ~5UL); }

    void set_item(Queue_item *i)
    {
      assert (!(Mword(i) & 5));
      set_unused((Mword)i | (get_unused() & 5));
    }

    bool blocked() const
    { return get_unused() & 1; }

    void block()
    { return set_unused(get_unused() | 1); }

    void unblock()
    { set_unused(get_unused() & ~1); }

    bool invalid() const
    { return get_unused() & 4; }

    void invalidate()
    { set_unused(get_unused() | 4); }
  };

  struct Queue_head_policy
  {
    typedef Lock_n_ptr Head_type;
    static Queue_item *head(Head_type const &h) { return h.item(); }
    static void set_head(Head_type &h, Queue_item *v) { h.set_item(v); }
  };

  typedef cxx::Sd_list<Queue_item, cxx::D_list_item_policy, Queue_head_policy> List;

  List _m;
};


//--------------------------------------------------------------------------
IMPLEMENTATION:

#include "assert.h"
#include "std_macros.h"

PUBLIC inline
Queue::Queue()
{ _m.head().init(); }

PUBLIC inline
Queue::Inner_lock *
Queue::q_lock()
{ return &_m.head(); }

PUBLIC inline NEEDS["assert.h"]
void
Queue::enqueue(Queue_item *i)
{
  // Queue i at the end of the list
  assert (i && !i->queued());
  assert (_m.head().test());
  i->_q = this;
  _m.push_back(i);
}

PUBLIC inline NEEDS["assert.h", "std_macros.h"]
bool
Queue::dequeue(Queue_item *i, Queue_item::Status reason)
{
  assert (_m.head().test());
  assert (i->queued());

  if (EXPECT_FALSE(i->_q != this))
    return false;

  _m.remove(i);
  i->_q = (Queue*)reason;
  return true;
}

PUBLIC inline
Queue_item *
Queue::first() const
{ return _m.front(); }

PUBLIC inline
bool
Queue::blocked() const
{ return _m.head().blocked(); }

PUBLIC inline NEEDS["assert.h"]
void
Queue::block()
{
  assert (_m.head().test());
  _m.head().block();
}

PUBLIC inline NEEDS["assert.h"]
void
Queue::unblock()
{
  assert (_m.head().test());
  _m.head().unblock();
}

PUBLIC inline NEEDS["assert.h"]
bool
Queue::invalid() const
{
  assert (_m.head().test());
  return _m.head().invalid();
}

PUBLIC inline NEEDS["assert.h"]
void
Queue::invalidate()
{
  assert (_m.head().test());
  _m.head().invalidate();
}

