INTERFACE: 

#include "lock_guard.h"
#include "switch_lock.h"

#ifdef NO_INSTRUMENT
#undef NO_INSTRUMENT
#endif
#define NO_INSTRUMENT __attribute__((no_instrument_function))

/** A wrapper for Switch_lock that works even when the threading system
    has not been intialized yet.
    This wrapper is necessary because most lock-protected objects are 
    initialized before the threading system has been fired up.
 */
class Helping_lock : private Switch_lock
{

public:
  using Switch_lock::Status;
  using Switch_lock::Not_locked;
  using Switch_lock::Locked;
  using Switch_lock::Invalid;

  using Switch_lock::invalidate;
  using Switch_lock::valid;
  using Switch_lock::wait_free;

  static bool threading_system_active;
};

#undef NO_INSTRUMENT
#define NO_INSTRUMENT

IMPLEMENTATION:

#include "globals.h"
#include "panic.h"
#include "std_macros.h"


/** Threading system activated. */
bool Helping_lock::threading_system_active = false;

/** Constructor. */
PUBLIC inline
Helping_lock::Helping_lock ()
{
  Switch_lock::initialize();
}

/** Acquire the lock with priority inheritance.
    @return true if we owned the lock already.  false otherwise.
 */
PUBLIC inline
Helping_lock::Status NO_INSTRUMENT
Helping_lock::test_and_set ()
{
  if (! threading_system_active) // still initializing?
    return Not_locked;
  
  return Switch_lock::test_and_set();
}

/** Acquire the lock with priority inheritance.
    If the lock is occupied, enqueue in list of helpers and lend CPU 
    to current lock owner until we are the lock owner.
 */
PUBLIC inline NEEDS ["panic.h"]
Helping_lock::Status NO_INSTRUMENT
Helping_lock::lock ()
{
  return test_and_set();
}

/** Is lock set?.
    @return true if lock is set.
 */
PUBLIC inline NEEDS["std_macros.h"]
Helping_lock::Status NO_INSTRUMENT
Helping_lock::test ()
{
  if (EXPECT_FALSE( ! threading_system_active) ) // still initializing?
    return Not_locked;

  return Switch_lock::test();
}

/** Free the lock.
    Return the CPU to helper or next lock owner, whoever has the higher 
    priority, given that thread's priority is higher that our's.
 */
PUBLIC inline NEEDS["std_macros.h"]
void NO_INSTRUMENT
Helping_lock::clear()
{
  if (EXPECT_FALSE( ! threading_system_active) ) // still initializing?
    return;

  Switch_lock::clear();
}

PUBLIC inline NEEDS[Helping_lock::clear]
void
Helping_lock::set(Status s)
{
  if (!s)
    clear();
}

/** Lock owner. 
    @return current owner of the lock.  0 if there is no owner.
 */
PUBLIC inline NEEDS["std_macros.h", "globals.h"]
Context* NO_INSTRUMENT
Helping_lock::lock_owner () const
{
  if (EXPECT_FALSE( ! threading_system_active) ) // still initializing?
    return current();

  return Switch_lock::lock_owner();
}
