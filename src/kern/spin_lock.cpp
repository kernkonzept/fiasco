INTERFACE:

#include "cpu_lock.h"
#include "types.h"

#include <cassert>

/**
 * Basic spin lock.
 *
 * Also disables lock IRQs for the time the lock is held.
 * In the UP case it is in fact just the Cpu_lock.
 *
 * The memory order guarantees provided by the spin lock are not limited to the
 * memory accesses within the critical section protected by the lock, but also
 * apply to accesses before and after the critical section.
 *
 * In other words, the following rule applies:
 * A CPU holding a lock sees all changes previously seen or made by any CPU
 * before it released that same lock.
 *
 * And also the reverse of the rule applies:
 * A CPU holding a lock does not see any changes subsequently made by any CPU
 * after it acquired that same lock.
 */
template<typename Lock_t = Small_atomic_int>
class Spin_lock : protected Cpu_lock
{
   Spin_lock(Spin_lock const &) = delete;
   Spin_lock operator = (Spin_lock const &) = delete;
};

//--------------------------------------------------------------------------
INTERFACE [!mp]:

EXTENSION class Spin_lock
{
public:
  constexpr Spin_lock() {}

  using Cpu_lock::Status;
  using Cpu_lock::test;
  using Cpu_lock::lock;
  using Cpu_lock::clear;
  using Cpu_lock::test_and_set;
  using Cpu_lock::set;

  bool is_locked() const { return !!test(); }
};

/**
 * Version of a spin lock that is colocated with another value.
 */
template< typename T >
class Spin_lock_coloc : public Spin_lock<Mword>
{
private:
  enum { Arch_lock = 1 };
  Mword _lock = 0;
};

/**
 * Optimized lock-guard policy for Spin_lock that does not disable IRQs to avoid
 * the overhead for cases where it is certain that IRQs are already disabled.
 */
template< typename LOCK >
struct No_cpu_lock_policy
{
  using Status = unsigned; // unused

  static Status test_and_set(LOCK *)
  {
    assert(cpu_lock.test());
    return 0;
  }

  static void set(LOCK *, Status)
  {}
};

//--------------------------------------------------------------------------
INTERFACE [mp]:

EXTENSION class Spin_lock
{
public:
  typedef Mword Status;
  /// Initialize spin lock in unlocked state.
  constexpr Spin_lock() : _lock(0) {}

  template< typename LOCK >
  friend struct No_cpu_lock_policy;

private:
  /**
   * Acquire the lock (architecture-specific).
   *
   * Attempts to atomically set the lock flag. If the lock flag is already set,
   * i.e. the lock is held by someone else, spin until the lock is released and
   * then can be acquired.
   *
   * \pre Must not be called by the current lock holder.
   *
   * \note Needs to provide memory acquire semantics and act as compiler barrier.
   *
   * \note When modifying the lock_arch implementation for an architecture,
   *       also consider the assembler copy of the function in tramp-mp.S.
   */
  void lock_arch();

  /**
   * Release the lock (architecture-specific).
   *
   * Atomically clears the lock flag.
   *
   * \pre Must only be called by the current lock holder.
   *
   * \note Needs to provide memory release semantics and act as compiler barrier.
   */
  void unlock_arch();

protected:
  Lock_t _lock;
};

/**
 * Version of a spin lock that is colocated with another value.
 */
template< typename T >
class Spin_lock_coloc : public Spin_lock<Mword>
{};

/**
 * Optimized lock-guard policy for Spin_lock that does not disable IRQs to avoid
 * the overhead for cases where it is certain that IRQs are already disabled.
 */
template< typename LOCK >
struct No_cpu_lock_policy
{
  using Status = unsigned; // unused

  static Status test_and_set(LOCK *l)
  {
    assert(cpu_lock.test());
    l->lock_arch();
    return 0;
  }

  static void set(LOCK *l, Status)
  { l->unlock_arch(); }
};

//--------------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC inline
template< typename T >
T
Spin_lock_coloc<T>::get_unused() const
{ return reinterpret_cast<T>(_lock & ~Arch_lock); }

PUBLIC inline
template< typename T >
void
Spin_lock_coloc<T>::set_unused(T val)
{ _lock = (_lock & Arch_lock) | reinterpret_cast<Mword>(val); }


//--------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "mem.h"

PUBLIC template<typename Lock_t> inline
typename Spin_lock<Lock_t>::Status
Spin_lock<Lock_t>::test() const
{
  return (!!cpu_lock.test()) | (_lock & Arch_lock);
}

PUBLIC template<typename Lock_t> inline NEEDS[Spin_lock::lock_arch, "mem.h"]
void
Spin_lock<Lock_t>::lock()
{
  assert(!cpu_lock.test());
  cpu_lock.lock();
  lock_arch();
}

PUBLIC template<typename Lock_t> inline NEEDS[Spin_lock::unlock_arch, "mem.h"]
void
Spin_lock<Lock_t>::clear()
{
  unlock_arch();
  Cpu_lock::clear();
}

PUBLIC template<typename Lock_t> inline NEEDS[Spin_lock::lock_arch, "mem.h"]
typename Spin_lock<Lock_t>::Status
Spin_lock<Lock_t>::test_and_set()
{
  Status s = !!cpu_lock.test();
  cpu_lock.lock();
  lock_arch();
  return s;
}

PUBLIC template<typename Lock_t> inline
void
Spin_lock<Lock_t>::set(Status s)
{
  if (!(s & Arch_lock))
    unlock_arch();

  if (!(s & 1))
    cpu_lock.clear();
}

/**
 * Returns whether the lock is set.
 *
 * This function is intended for use in asserts. It is necessary because the
 * Status returned by `test()` encodes not only the status of the Spin_lock, but
 * also the status of the cpu lock.
 */
PUBLIC template<typename Lock_t> inline
bool
Spin_lock<Lock_t>::is_locked() const
{
  return _lock & Arch_lock;
}
