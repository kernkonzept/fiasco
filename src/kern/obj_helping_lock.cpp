INTERFACE: 

#include "switch_lock.h"

#ifdef NO_INSTRUMENT
#undef NO_INSTRUMENT
#endif
#define NO_INSTRUMENT __attribute__((no_instrument_function))

class Obj_lock_valid
{
};

/** A wrapper for Switch_lock that works even when the threading system
    has not been intialized yet.
    This wrapper is necessary because most lock-protected objects are 
    initialized before the threading system has been fired up.
 */
class Obj_helping_lock : private Switch_lock<Obj_lock_valid>
{
private:
  typedef Switch_lock<Obj_lock_valid> Lock;

public:
  using Lock::Status;
  using Lock::Invalid;
  using Lock::Locked;
  using Lock::Not_locked;

  using Lock::Lock_context;
  
  using Lock::test_and_set;
  using Lock::test_and_set_dirty;
  using Lock::lock;
  using Lock::lock_dirty;
  using Lock::clear;
  using Lock::clear_dirty;

  using Lock::test;
  using Lock::lock_owner;

  /*
   * Can use the Switch_lock version, because we assume the context
   * is invalid during switch_dirty and we do not need to consider it
   * for scheduling any more.
   */
  using Lock::clear_no_switch_dirty;
  using Lock::switch_dirty;

private:
  friend class Obj_lock_valid;
  bool _valid;

};

#undef NO_INSTRUMENT
#define NO_INSTRUMENT

IMPLEMENTATION:

PUBLIC inline
Obj_helping_lock::Obj_helping_lock() 
: _valid(true)
{}

PUBLIC inline static
bool Obj_lock_valid::valid(Switch_lock<Obj_lock_valid> const *lock) 
{
  Obj_helping_lock const *l = static_cast<Obj_helping_lock const*>(lock);
  return l->_valid;
}

PUBLIC inline
void
Obj_helping_lock::invalidate() 
{ _valid = false; }
