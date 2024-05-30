INTERFACE:

#include "types.h"

class Ref_cnt_obj
{
public:
  Ref_cnt_obj() : _ref_cnt(0) {}
private:
  Smword _ref_cnt;
};

template<typename T>
class Ref_ptr
{
public:
  void reset(T *n = 0)
  {
    T *old = _o;
    _o = n;
    if (n)
      n->inc_ref();

    if (old && old->dec_ref() == 0)
      delete old;
  }

  T *release()
  {
    auto r = _o;
    _o = 0;
    return r;
  }

  T *get() const { return _o; }

  Ref_ptr() : _o(0) {}
  explicit Ref_ptr(T *p) : _o(p)
  {
    if (_o)
      _o->inc_ref();
  }

  Ref_ptr(Ref_ptr const &o) : _o(o._o)
  {
    if (_o)
      _o->inc_ref();
  }

  Ref_ptr(Ref_ptr &&o) : _o(o._o)
  {
    o._o = 0;
  }

  ~Ref_ptr() noexcept
  { reset(); }

  Ref_ptr &operator = (Ref_ptr const &o)
  {
    if (&o == this)
      return *this;

    reset(o._o);
    return *this;
  }

  Ref_ptr &operator = (Ref_ptr &&o)
  {
    if (&o == this)
      return *this;

    reset();
    _o = o._o;
    o._o = 0;

    return *this;
  }

  T *operator -> () const { return get(); }

  explicit operator bool () const { return _o != 0; }

private:
  T *_o;
};


// -------------------------------------------------------------------------
IMPLEMENTATION:

#include "atomic.h"

/**
 * Return the current value of the reference counter.
 *
 * \return The current value of the reference counter.
 */
PUBLIC inline
Smword
Ref_cnt_obj::ref_cnt() const
{ return _ref_cnt; }

/**
 * Atomically increments the reference counter by one.
 *
 * \param from_zero  On `false`, do not increment the counter if the counter is
 *                   currently zero. On `true`, always increment.
 * \retval false  The incrementation was not performed because `from_zero=false`
 *                was passed and the counter value was 0.
 * \retval true   The incrementation was performed.
 *
 * \note There is no protection against overflow of the counter.
 *
 */
PUBLIC inline NEEDS["atomic.h"]
bool
Ref_cnt_obj::inc_ref(bool from_zero = true)
{
  Smword old;
  do
    {
      old = _ref_cnt;
      if (!from_zero && !old)
        return false;
    }
  while (!cas(&_ref_cnt, old, old + 1));
  return true;
}

/**
 * Atomically decrement the reference counter by one and return the resulting
 * counter value.
 *
 * A result of 0 usually leads to some actions on the caller's side, usually
 * removing the corresponding object. Therefore warn if the caller doesn't
 * evaluate the result.
 *
 * \note There is no protection against reaching negative reference counter
 *       values.
 *
 * \return The reference counter value after decrementing.
 */
PUBLIC inline NEEDS["atomic.h"]
Smword FIASCO_WARN_RESULT
Ref_cnt_obj::dec_ref()
{
  Smword old;
  do
    old = _ref_cnt;
  while (!cas(&_ref_cnt, old, old - 1));

  return old - 1;
}
