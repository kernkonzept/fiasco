INTERFACE:

#include <cxx/type_traits>
#include "types.h"

/**
 * Global CPU lock. When held, IRQs are disabled on the current CPU
 * (preventing nested IRQ handling, and preventing the current
 * thread from being preempted).  It must only be held for very short
 * amounts of time.
 *
 * A generic (cli, sti) implementation of the lock can be found in
 * cpu_lock-generic.cpp.
 */
class Cpu_lock
{
public:
  /// The return type of test methods
  typedef Mword Status;

  enum : Status { Not_locked = 0 };

  /// ctor.
  constexpr Cpu_lock();

  /**
   * Test if the lock is already held.
   * @return 0 if the lock is not held, not 0 if it already is held.
   */
  Status test() const;

  /**
   * Acquire the CPU lock.
   * The CPU lock disables IRQs. It should be held only for a very
   * short amount of time.
   */
  void lock();

  /**
   * Release the CPU lock.
   */
  void clear();

  /**
   * Acquire the CPU lock and return the old status.
   * @return something else than 0 if the lock was already held and
   *   0 if it was not held.
   */
  Status test_and_set();

  /**
   * Clear the CPU lock and return the old status.
   * @return something else than 0 if the lock was held before and
   *   0 if it was not held.
   */
  Status test_and_clear();

  /**
   * Set the CPU lock according to the given status.
   * @param state the state to set (0 clear, else lock).
   */
  void set(Status state);

private:
  /// Default copy constructor not implemented.
  Cpu_lock (const Cpu_lock&);
};

/**
 * The global CPU lock, contains the locking data necessary for some
 * special implementations.
 *
 * The object must never be dereferenced. In case this is still done, the
 * address is carefully chosen to be neither nullptr nor ~0UL, another pointer
 * value for non-existing objects.
 */
#define cpu_lock (*reinterpret_cast<Cpu_lock*>(~0UL - 255UL))

static_assert(cxx::is_empty_v<Cpu_lock>, "Cpu_lock must not have any members");

IMPLEMENTATION:

#include "static_init.h"

IMPLEMENT inline //NEEDS [Cpu_lock::lock, Cpu_lock::test]
Cpu_lock::Status Cpu_lock::test_and_set()
{
  Status ret = test();
  lock();
  return ret;
}

IMPLEMENT inline //NEEDS [Cpu_lock::clear, Cpu_lock::test]
Cpu_lock::Status Cpu_lock::test_and_clear()
{
  Status ret = test();
  clear();
  return ret;
}

IMPLEMENT inline //NEEDS [Cpu_lock::lock, Cpu_lock::clear]
void Cpu_lock::set(Cpu_lock::Status state)
{
  if (state)
    lock();
  else
    clear();
}
