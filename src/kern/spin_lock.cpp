INTERFACE:

#include "cpu_lock.h"
#include "types.h"

class Spin_lock_base : protected Cpu_lock
{
public:
  enum Lock_init { Unlocked = 0 };
};

/**
 * \brief Basic spin lock.
 *
 * Also disables lock IRQs for the time the lock is held.
 * In the UP case it is in fact just the Cpu_lock.
 */
template<typename Lock_t = Small_atomic_int>
class Spin_lock : public Spin_lock_base
{
};

//--------------------------------------------------------------------------
INTERFACE [!mp]:

EXTENSION class Spin_lock
{
public:
  Spin_lock() {}
  explicit Spin_lock(Lock_init) {}
  void init() {}

  using Cpu_lock::Status;
  using Cpu_lock::test;
  using Cpu_lock::lock;
  using Cpu_lock::clear;
  using Cpu_lock::test_and_set;
  using Cpu_lock::set;

};

/**
 * \brief Version of a spin lock that is colocated with another value.
 */
template< typename T >
class Spin_lock_coloc : public Spin_lock<Mword>
{
private:
  enum { Arch_lock = 1 };
  Mword _lock;
};


//--------------------------------------------------------------------------
INTERFACE [mp]:

EXTENSION class Spin_lock
{
public:
  typedef Mword Status;
  Spin_lock() {}
  explicit Spin_lock(Lock_init i) : _lock((i == Unlocked) ? 0 : Arch_lock) {}

protected:
  Lock_t _lock;
};

/**
 * \brief Version of a spin lock that is colocated with another value.
 */
template< typename T >
class Spin_lock_coloc : public Spin_lock<Mword>
{
};

//--------------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC inline
template< typename T >
Spin_lock_coloc<T>::Spin_lock_coloc() {}

PUBLIC inline
template< typename T >
Spin_lock_coloc<T>::Spin_lock_coloc(Lock_init i) : Spin_lock<Mword>(i) {}

PUBLIC inline
template< typename T >
T
Spin_lock_coloc<T>::get_unused() const
{ return (T)(_lock & ~Arch_lock); }

PUBLIC inline
template< typename T >
void
Spin_lock_coloc<T>::set_unused(T val)
{ _lock = (_lock & Arch_lock) | (Mword)val; }


//--------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include <cassert>
#include "mem.h"

PUBLIC template<typename Lock_t> inline
void
Spin_lock<Lock_t>::init()
{
  _lock = 0;
}

PUBLIC template<typename Lock_t> inline
typename Spin_lock<Lock_t>::Status
Spin_lock<Lock_t>::test() const
{
  return (!!cpu_lock.test()) | (_lock & Arch_lock);
}

PUBLIC template<typename Lock_t> inline NEEDS[<cassert>, Spin_lock::lock_arch, "mem.h"]
void
Spin_lock<Lock_t>::lock()
{
  assert(!cpu_lock.test());
  cpu_lock.lock();
  lock_arch();
  Mem::mp_acquire();
}

PUBLIC template<typename Lock_t> inline NEEDS[Spin_lock::unlock_arch, "mem.h"]
void
Spin_lock<Lock_t>::clear()
{
  Mem::mp_release();
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
  Mem::mp_acquire();
  return s;
}

PUBLIC template<typename Lock_t> inline
void
Spin_lock<Lock_t>::set(Status s)
{
  Mem::mp_release();
  if (!(s & Arch_lock))
    unlock_arch();

  if (!(s & 1))
    cpu_lock.clear();
}


