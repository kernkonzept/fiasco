#pragma once

#include "types.h"

class Ram_quota;

template<typename T = Ram_quota>
class Auto_quota
{
public:
  Auto_quota(Auto_quota const &) = delete;
  Auto_quota operator = (Auto_quota const &) = delete;

  Auto_quota(T *quota, unsigned long size)
  : _quota(nullptr), _bytes(size)
  {
    if (quota->alloc(size))
      _quota = quota;
  }

  Auto_quota(T *quota, Order order)
  : _quota(nullptr), _bytes(Bytes(1) << order)
  {
    if (quota->alloc(Bytes(1) << order))
      _quota = quota;
  }

  Auto_quota(T *quota, Bytes size)
  : _quota(nullptr), _bytes(size)
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

    _quota = nullptr;
  }

  ~Auto_quota() { reset(); }

  T *release()
  {
    T *q = _quota;
    _quota = nullptr;
    return q;
  }

  explicit operator bool () const
  { return _quota != nullptr; }

private:
  void reset(T *quota, Bytes size)
  {
    if (_quota)
      _quota->free(_bytes);

    _quota = quota;
    _bytes = size;
  }

  T *_quota;
  Bytes _bytes;
};
