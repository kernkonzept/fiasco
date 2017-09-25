INTERFACE:

#include "types.h"

class Simple_lock
{
  Mword _lock;
};

IMPLEMENTATION:

#include "atomic.h"

PUBLIC 
inline NEEDS ["atomic.h"]
bool Simple_lock::test_and_set()
{
  return smp_tas(&_lock);
}

PUBLIC
inline 
void Simple_lock::clear()
{
  _lock = 0;
}

PUBLIC
inline 
void Simple_lock::set()
{
  _lock = 1;
}

PUBLIC
inline 
bool Simple_lock::test()
{
  return _lock;
}
