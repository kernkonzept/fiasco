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

PUBLIC inline
Smword
Ref_cnt_obj::ref_cnt() const
{ return _ref_cnt; }

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
  while (!mp_cas(&_ref_cnt, old, old + 1));
  return true;
}

PUBLIC inline NEEDS["atomic.h"]
Smword
Ref_cnt_obj::dec_ref()
{
  Smword old;
  do
    old = _ref_cnt;
  while (!mp_cas(&_ref_cnt, old, old - 1));

  return old - 1;
}
