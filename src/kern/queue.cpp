INTERFACE:

#include "spin_lock.h"
#include "queue_item.h"
#include "assert.h"
#include <cxx/dlist>

class Queue
{
public:
  typedef Spin_lock_coloc<Queue_item *> Inner_lock;

private:
  class Lock_n_ptr : public Inner_lock
  {
    static_assert(__alignof__(Queue_item) >= 8,
                  "Lock_n_ptr uses 3 LSBs of item pointer");

  public:
    Queue_item *item() const
    { return get_unused(); }

    void set_item(Queue_item *i)
    { set_unused(i); }
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
Queue::dequeue(Queue_item *i)
{
  assert (_m.head().test());
  assert (i->queued());

  if (EXPECT_FALSE(i->_q != this))
    return false;

  _m.remove(i);
  return true;
}

PUBLIC inline
Queue_item *
Queue::first() const
{ return _m.front(); }
