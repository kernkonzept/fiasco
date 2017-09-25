#pragma once

class Ram_quota;

template<typename T = Ram_quota>
class Auto_quota
{
private:
  struct _unspec;
  typedef _unspec* _unspec_bool_type;

public:
  Auto_quota(Auto_quota const &) = delete;
  Auto_quota operator = (Auto_quota const &) = delete;

  Auto_quota(T *quota, unsigned long size)
  : _quota(0), _bytes(size)
  {
    if (quota->alloc(size))
      _quota = quota;
  }

  Auto_quota(Auto_quota &&o) : _quota(o.release()), _bytes(o._bytes) {}
  Auto_quota &operator = (Auto_quota &&o)
  {
    if (*this == o)
      return *this;

    reset(o.release(), o._bytes);
    return *this;
  }

  void reset()
  {
    if (_quota)
      _quota->free(_bytes);

    _quota = 0;
  }

  ~Auto_quota() { reset(); }

  T *release()
  {
    T *q = _quota;
    _quota = 0;
    return q;
  }

  operator _unspec_bool_type () const
  { return reinterpret_cast<_unspec_bool_type>(_quota); }

private:
  void reset(T *quota, unsigned long size)
  {
    if (_quota)
      _quota->free(_bytes);

    _quota = quota;
    _bytes = size;
  }

  T *_quota;
  unsigned long _bytes;
};
