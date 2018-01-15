#pragma once

namespace cxx {

class Simple_alloc
{
public:
  Simple_alloc() = default;

  Simple_alloc(void *block, unsigned size)
  : _p((unsigned long)block), _s(size)
  {}

  Simple_alloc(unsigned long block_addr, unsigned size)
  : _p(block_addr), _s(size)
  {}

  void *alloc_bytes(unsigned size, unsigned long align = 1)
  {
    if (size > _s)
      return nullptr;

    align -= 1;
    unsigned long x = (_p + align) & ~align;
    if ((x - _p) >= _s)
      return nullptr;

    if ((x - _p + size) > _s)
      return nullptr;

    _s -= x - _p + size;
    _p = x + size;
    return (void *)x;
  }

  template<typename T>
  T *alloc(unsigned count = 1, unsigned align = alignof(T))
  {
    return (T *)alloc_bytes(sizeof (T) * count, align);
  }

  void *block() const { return (void *)_p; }
  unsigned size() const { return _s; }

private:
  unsigned long _p = 0;
  unsigned _s = 0;
};

}
