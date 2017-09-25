INTERFACE:

#include "atomic.h"

template< bool LARGE, unsigned BITS >
class Bitmap_base;

// Implementation for bitmaps bigger than sizeof(unsigned long) * 8
// Derived classes have to make sure to provide the storage space for
// holding the bitmap.
template<unsigned BITS>
class Bitmap_base< true, BITS >
{
public:
  enum {
    Bpl      = sizeof(unsigned long) * 8,
    Nr_elems = (BITS + Bpl - 1) / Bpl,
  };

  void bit(unsigned long bit, bool on)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    _bits[idx] = (_bits[idx] & ~(1UL << b)) | ((unsigned long)on << b);
  }

  void clear_bit(unsigned long bit)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    _bits[idx] &= ~(1UL << b);
  }

  void set_bit(unsigned long bit)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    _bits[idx] |= (1UL << b);
  }

  unsigned long operator [] (unsigned long bit) const
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    return _bits[idx] & (1UL << b);
  }

  bool atomic_get_and_clear(unsigned long bit)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    unsigned long v;

    if (!(_bits[idx] & (1UL << b)))
      return false;

    do
      {
        v = _bits[idx];
      }
    while (!mp_cas(&_bits[idx], v, v & ~(1UL << b)));

    return v & (1UL << b);
  }

  bool atomic_get_and_set(unsigned long bit)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    unsigned long v;
    do
      {
        v = _bits[idx];
      }
    while (!mp_cas(&_bits[idx], v, v | (1UL << b)));

    return v & (1UL << b);
  }

  void atomic_set_bit(unsigned long bit)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    atomic_mp_or(&_bits[idx], 1UL << b);
  }

  void atomic_clear_bit(unsigned long bit)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    atomic_mp_and(&_bits[idx], ~(1UL << b));
  }

  void clear_all()
  {
    for (unsigned i = 0; i < Nr_elems; ++i)
      _bits[i] = 0;
  }

  bool is_empty() const
  {
    for (unsigned i = 0; i < Nr_elems; ++i)
      if (_bits[i])
	return false;
    return true;
  }

  void atomic_or(Bitmap_base const &r)
  {
    for (unsigned i = 0; i < Nr_elems; ++i)
      atomic_mp_or(&_bits[i], r._bits[i]);
  }

protected:
  template< bool BIG, unsigned BTS > friend class Bitmap_base;

  Bitmap_base() {}
  Bitmap_base(Bitmap_base const &) = delete;
  Bitmap_base &operator = (Bitmap_base const &) = delete;


  void _or(Bitmap_base const &r)
  {
    for (unsigned i = 0; i < Nr_elems; ++i)
      _bits[i] |= r._bits[i];
  }

  template<unsigned SOURCE_BITS>
  void _copy(Bitmap_base<true, SOURCE_BITS> const &s)
  {
    static_assert(BITS <= SOURCE_BITS,
                  "cannot copy smaller bitmap into bigger one");
    __builtin_memcpy(_bits, s._bits, Nr_elems * sizeof(_bits[0]));
  }

  unsigned long _bits[Nr_elems];
};

// Implementation for a bitmap up to sizeof(unsigned long) * 8 bits
template<unsigned BITS>
class Bitmap_base<false, BITS>
{
public:
  void bit(unsigned long bit, bool on)
  {
    _bits = (_bits & ~(1UL << bit)) | ((unsigned long)on << bit);
  }

  void clear_bit(unsigned long bit)
  {
    _bits &= ~(1UL << bit);
  }

  void set_bit(unsigned long bit)
  {
    _bits |= 1UL << bit;
  }

  unsigned long operator [] (unsigned long bit) const
  {
    return _bits & (1UL << bit);
  }

  bool atomic_get_and_clear(unsigned long bit)
  {
    if (!(_bits & (1UL << bit)))
      return false;

    unsigned long v;
    do
      {
        v = _bits;
      }
    while (!mp_cas(&_bits, v, v & ~(1UL << bit)));

    return v & (1UL << bit);
  }

  bool atomic_get_and_set(unsigned long bit)
  {
    unsigned long v;
    do
      {
        v = _bits;
      }
    while (!mp_cas(&_bits, v, v | (1UL << bit)));

    return v & (1UL << bit);
  }

  void atomic_set_bit(unsigned long bit)
  {
    atomic_mp_or(&_bits, 1UL << bit);
  }

  void atomic_clear_bit(unsigned long bit)
  {
    atomic_mp_and(&_bits, ~(1UL << bit));
  }

  void clear_all()
  {
    _bits = 0;
  }

  bool is_empty() const
  {
    return !_bits;
  }

  void atomic_or(Bitmap_base const &r)
  {
    atomic_mp_or(&_bits, r._bits);
  }



protected:
  template< bool BIG, unsigned BTS > friend class Bitmap_base;
  enum
  {
    Bpl      = sizeof(unsigned long) * 8,
    Nr_elems = 1,
  };
  unsigned long _bits;

  Bitmap_base() {}
  Bitmap_base(Bitmap_base const &) = delete;
  Bitmap_base &operator = (Bitmap_base const &) = delete;

  void _or(Bitmap_base const &r)
  {
    _bits |= r._bits;
  }

  void _copy(Bitmap_base const &s)
  { _bits = s._bits; }

  template<unsigned SOURCE_BITS>
  void _copy(Bitmap_base<false, SOURCE_BITS> const &s)
  {
    static_assert(BITS <= SOURCE_BITS,
                  "cannot copy smaller bitmap into bigger one");
    _bits = s._bits;
  }

  template<unsigned SOURCE_BITS>
  void _copy(Bitmap_base<true, SOURCE_BITS> const &s)
  {
    static_assert(BITS <= SOURCE_BITS,
                  "cannot copy smaller bitmap into bigger one");
    _bits = s._bits[0];
  }

};

template<unsigned BITS>
class Bitmap : public Bitmap_base< (BITS > sizeof(unsigned long) * 8), BITS >
{
public:
  Bitmap() {}
  Bitmap(Bitmap const &o) : Bitmap_base< (BITS > sizeof(unsigned long) * 8), BITS >()
  { this->_copy(o); }

  Bitmap &operator = (Bitmap const &o)
  {
    this->_copy(o);
    return *this;
  }

  template<unsigned SBITS>
  Bitmap &operator = (Bitmap<SBITS> const &o)
  {
    this->_copy(o);
    return *this;
  }

  Bitmap &operator |= (Bitmap const &o)
  {
    this->_or(o);
    return *this;
  }
};
