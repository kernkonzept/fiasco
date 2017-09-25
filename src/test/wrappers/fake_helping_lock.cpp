INTERFACE: 

#include "lock_guard.h"
#include "simple_lock.h"

class Context;

class Helping_lock : public Simple_lock
{
  Context* _owner;
};


IMPLEMENTATION:

#include "globals.h"

PUBLIC void
Helping_lock::lock()
{
  test_and_set();
}

PUBLIC Context*
Helping_lock::lock_owner()
{
  if (test())
    return current();
  else
    return 0;
}
