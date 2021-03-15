INTERFACE:

#include <cxx/dlist>

class Queue;

class Queue_item : public cxx::D_list_item
{
  friend class Queue;

private:
  Queue *_q;
} __attribute__((aligned(16)));


//--------------------------------------------------------------------------
IMPLEMENTATION:

#include "assert.h"

PUBLIC inline
bool
Queue_item::queued() const
{ return cxx::D_list_cyclic<Queue_item>::in_list(this); }

PUBLIC inline NEEDS["assert.h"]
Queue *
Queue_item::queue() const
{
  assert (queued());
  return _q;
}

