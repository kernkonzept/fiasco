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
  static Status test_and_set(LOCK *l) { l->clear(); return LOCK::Locked; }
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
  typedef typename cxx::remove_pointer<typename cxx::remove_reference<LOCK>::type>::type Lock;
  typedef POLICY<Lock> Policy;

  Lock *_lock;
  typename Policy::Status _state;

  Lock_guard(Lock_guard &) = delete;
  Lock_guard &operator = (Lock_guard &) = delete;
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
Lock_guard<LOCK, POLICY>
Lock_guard<LOCK, POLICY>::operator = (Lock_guard &&l)
{
  reset();
  _lock = l._lock;
  _state = l._state;
  l.release();
}


inline
template<template<typename L> class POLICY = Lock_guard_regular_policy, typename LOCK>
Lock_guard<LOCK, POLICY> lock_guard(LOCK &lock)
{ return Lock_guard<LOCK, POLICY>(&lock); }

inline
template<template<typename L> class POLICY = Lock_guard_regular_policy, typename LOCK>
Lock_guard<LOCK, POLICY> lock_guard(LOCK *lock)
{ return Lock_guard<LOCK, POLICY>(lock); }

inline
template<template<typename L> class POLICY = Lock_guard_regular_policy, typename LOCK>
Lock_guard<LOCK, POLICY> lock_guard_dont_lock(LOCK &)
{ return Lock_guard<LOCK, POLICY>(); }

inline
template<template<typename L> class POLICY = Lock_guard_regular_policy, typename LOCK>
Lock_guard<LOCK, POLICY> lock_guard_dont_lock(LOCK *)
{ return Lock_guard<LOCK, POLICY>(); }

PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
void
Lock_guard<LOCK, POLICY>::lock(Lock *l)
{
  _lock = l;
  _state = Policy::test_and_set(l);
}

PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
bool
Lock_guard<LOCK, POLICY>::check_and_lock(Lock *l)
{
  _lock = l;
  _state = Policy::test_and_set(l);
  return _state != Lock::Invalid;
}

PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
bool
Lock_guard<LOCK, POLICY>::try_lock(Lock *l)
{
  _state = Policy::test_and_set(l);
  switch (_state)
    {
    case Lock::Locked:
      return true;
    case Lock::Not_locked:
      _lock = l;			// Was not locked -- unlock.
      return true;
    default:
      return false; // Error case -- lock not existent
    }
}

PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
void
Lock_guard<LOCK, POLICY>::release()
{
  _lock = 0;
}

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

PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
Lock_guard<LOCK, POLICY>::~Lock_guard()
{
  if (_lock)
    Policy::set(_lock, _state);
}

PUBLIC template<typename LOCK, template< typename L > class POLICY>
inline
bool
Lock_guard<LOCK, POLICY>::was_set(void)
{
  return _state; //!_lock;
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
