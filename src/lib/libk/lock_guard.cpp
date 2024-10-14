INTERFACE:
#include <cxx/type_traits>

//
// Regular lock-guard policy, lock on ctor, unlock/reset in dtor
//
template< typename LOCK >
struct Lock_guard_regular_policy
{
  typedef typename LOCK::Status Status;
  static Status test_and_set(LOCK *l) { return l->test_and_set(); }
  static void set(LOCK *l, Status s) { l->set(s); }
};

//
// Inverse lock-guard policy, unlock in ctor, lock in dtor
// NOTE: this is applicable only to some locks (e.g., the cpu lock)
//
template<typename LOCK>
struct Lock_guard_inverse_policy : private LOCK
{
  typedef typename LOCK::Status Status;
  static Status test_and_set(LOCK *l) { return l->test_and_clear(); }
  static void set(LOCK *l, Status s) { l->set(s); }
};


//
// Lock_guard: a guard object using a lock such as helping_lock_t
//
template<
  typename LOCK,
  template< typename L > class POLICY = Lock_guard_regular_policy >
class Lock_guard
{
protected:
  typedef cxx::remove_pointer_t<cxx::remove_reference_t<LOCK>> Lock;
  typedef POLICY<Lock> Policy;

  Lock *_lock;
  typename Policy::Status _state;

  Lock_guard(Lock_guard &) = delete;
  Lock_guard &operator = (Lock_guard &) = delete;
};

/**
 * A lock guard for locks that have an `Invalid` state.
 *
 * This guard does not have a default constructor. A lock must be passed on
 * construction and is immediately attempted to lock. Hence, after construction,
 * the validity of the lock is clear and can be checked via is_valid(). This
 * should usually be done.
 */
template<
  typename LOCK,
  template< typename L > class POLICY = Lock_guard_regular_policy >
class Switch_lock_guard : public Lock_guard<LOCK, POLICY>
{
  Switch_lock_guard() = delete;
  Switch_lock_guard(Switch_lock_guard &) = delete;
  Switch_lock_guard &operator = (Switch_lock_guard &) = delete;
public:
  Switch_lock_guard &operator = (Switch_lock_guard &&) = default;
};

template< typename LOCK>
class Lock_guard_2
{
  LOCK *_l1, *_l2;
  typename LOCK::Status _state1, _state2;
};

IMPLEMENTATION:


PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
Lock_guard<LOCK, POLICY>::Lock_guard()
  : _lock(0)
{}

PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline explicit
Lock_guard<LOCK, POLICY>::Lock_guard(Lock *l)
  : _lock(l)
{
  _state = Policy::test_and_set(_lock);
}

PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
Lock_guard<LOCK, POLICY>::Lock_guard(Lock_guard &&l)
  : _lock(l._lock), _state(l._state)
{
  l.release();
}

PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
Lock_guard<LOCK, POLICY>&
Lock_guard<LOCK, POLICY>::operator = (Lock_guard &&l)
{
  reset();
  _lock = l._lock;
  _state = l._state;
  l.release();
  return *this;
}


/**
 * Attach to a lock, acquire it and release it on destruction.
 */
inline
template<template<typename L> class POLICY = Lock_guard_regular_policy, typename LOCK>
Lock_guard<LOCK, POLICY> lock_guard(LOCK &lock)
{ return Lock_guard<LOCK, POLICY>(&lock); }

/**
 * Attach to a lock, acquire it and release it on destruction.
 */
inline
template<template<typename L> class POLICY = Lock_guard_regular_policy, typename LOCK>
Lock_guard<LOCK, POLICY> lock_guard(LOCK *lock)
{ return Lock_guard<LOCK, POLICY>(lock); }

/**
 * Detach from the lock.
 */
PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
void
Lock_guard<LOCK, POLICY>::release()
{
  _lock = 0;
}

/**
 * Restore the lock state to the state before the lock was taken and detach from
 * the lock.
 */
PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
void
Lock_guard<LOCK, POLICY>::reset()
{
  if (_lock)
    {
      Policy::set(_lock, _state);
      _lock = 0;
    }
}

/**
 * Restore the lock state to the state before the lock was taken.
 */
PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
Lock_guard<LOCK, POLICY>::~Lock_guard()
{
  if (_lock)
    Policy::set(_lock, _state);
}


/// \copydoc switch_lock_guard(LOCK &lock)
PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline explicit
Switch_lock_guard<LOCK, POLICY>::Switch_lock_guard(typename Lock_guard<LOCK, POLICY>::Lock *l)
  : Lock_guard<LOCK, POLICY>(l)
{}

/**
 * Attach to a lock, acquire it and release it on destruction.
 *
 * If the lock is invalid, the acquisition fails. This should usually be checked
 * via #Switch_lock_guard::is_valid().
 */
inline
template<template<typename L> class POLICY = Lock_guard_regular_policy, typename LOCK>
Switch_lock_guard<LOCK, POLICY> switch_lock_guard(LOCK &lock)
{ return Switch_lock_guard<LOCK, POLICY>(&lock); }

/// \copydoc switch_lock_guard(LOCK &lock)
inline
template<template<typename L> class POLICY = Lock_guard_regular_policy, typename LOCK>
Switch_lock_guard<LOCK, POLICY> switch_lock_guard(LOCK *lock)
{ return Switch_lock_guard<LOCK, POLICY>(lock); }

/**
 * Check if the lock is valid.
 *
 * \pre The underlying lock type must provide an `Invalid` state.
 */
PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
bool
Switch_lock_guard<LOCK, POLICY>::is_valid()
{
  return this->_state != Lock_guard<LOCK, POLICY>::Lock::Invalid;
}


PUBLIC template<typename LOCK>
inline
Lock_guard_2<LOCK>::Lock_guard_2()
  : _l1(0), _l2(0)
{}

PUBLIC template<typename LOCK>
inline
Lock_guard_2<LOCK>::Lock_guard_2(LOCK *l1, LOCK *l2)
  : _l1(l1 < l2 ? l1 : l2), _l2(l1 < l2 ? l2 : l1)
{
  _state1 = _l1->test_and_set();
  if (_l1 == _l2)
    _l2 = 0;
  else
    _state2 = _l2->test_and_set();
}


PUBLIC template<typename LOCK>
inline
void
Lock_guard_2<LOCK>::lock(LOCK *l1, LOCK *l2)
{
  _l1 = l1 < l2 ? l1 : l2;
  _l2 = l1 < l2 ? l2 : l1;
  _state1 = _l1->test_and_set();
  if (_l1 == _l2)
    _l2 = 0;
  else 
    _state2 = _l2->test_and_set();
}

PUBLIC template<typename LOCK>
inline
bool
Lock_guard_2<LOCK>::check_and_lock(LOCK *l1, LOCK *l2)
{
  _l1 = l1 < l2 ? l1 : l2;
  _l2 = l1 < l2 ? l2 : l1;
  if ((_state1 = _l1->test_and_set()) == LOCK::Invalid)
    {
      _l1 = _l2 = 0;
      return false;
    }

  if (_l1 == _l2)
    _l2 = 0;
  else if ((_state2 = _l2->test_and_set()) == LOCK::Invalid)
    {
      _l2 = 0;
      return false;
    }

  return true;
}

PUBLIC template<typename LOCK>
inline
Lock_guard_2<LOCK>::~Lock_guard_2()
{
  if (_l2)
    _l2->set(_state2);

  if (_l1)
    _l1->set(_state1);
}
