INTERFACE:

#include "atomic.h"
#include <cxx/type_traits>

template< bool LARGE, unsigned BITS >
class Bitmap_base;

template< typename STORAGE_TYPE >
class Bitmap_base_base
{
public:
  typedef typename cxx::remove_pointer<typename cxx::remove_all_extents<STORAGE_TYPE>::type>::type
    Bitmap_elem_type;

  enum { Bpl = sizeof(Bitmap_elem_type) * 8 };

  static constexpr unsigned long
  nr_elems(unsigned long nr_bits)
  { return (nr_bits + Bpl - 1) / Bpl; }

  static constexpr unsigned long
  size_in_bytes(unsigned long nr_bits)
  { return nr_elems(nr_bits) * sizeof(Bitmap_elem_type); }

  void bit(unsigned long bit, bool on)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    this->_bits[idx] = (this->_bits[idx] & ~(1UL << b)) | ((unsigned long)on << b);
  }

  void clear_bit(unsigned long bit)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    this->_bits[idx] &= ~(1UL << b);
  }

  void set_bit(unsigned long bit)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    this->_bits[idx] |= (1UL << b);
  }

  unsigned long operator [] (unsigned long bit) const
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    return this->_bits[idx] & (1UL << b);
  }

protected:
  STORAGE_TYPE _bits;
};

template<unsigned BITS>
struct Bitmap_base_base_x :
  Bitmap_base_base<unsigned long [(BITS + sizeof(unsigned long) * 8 - 1) / (sizeof(unsigned long) * 8)]>
{};


// Implementation for bitmaps bigger than sizeof(unsigned long) * 8
// Derived classes have to make sure to provide the storage space for
// holding the bitmap.
template<unsigned BITS>
class Bitmap_base< true, BITS > : public Bitmap_base_base_x<BITS>
{
public:
  enum {
    Bpl      = Bitmap_base_base_x<BITS>::Bpl,
    Nr_elems = (BITS + Bpl - 1) / Bpl,
  };

  bool atomic_get_and_clear(unsigned long bit)
  {
    static_assert (Bitmap_base_base_x<BITS>::Bpl == sizeof(unsigned long) * 8,
                   "invalid type of Bitmap_base_base");

    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    unsigned long v;

    if (!(this->_bits[idx] & (1UL << b)))
      return false;

    do
      {
        v = this->_bits[idx];
      }
    while (!mp_cas(&this->_bits[idx], v, v & ~(1UL << b)));

    return v & (1UL << b);
  }

  bool atomic_get_and_set(unsigned long bit)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    unsigned long v;
    do
      {
        v = this->_bits[idx];
      }
    while (!mp_cas(&this->_bits[idx], v, v | (1UL << b)));

    return v & (1UL << b);
  }

  void atomic_set_bit(unsigned long bit)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    atomic_mp_or(&this->_bits[idx], 1UL << b);
  }

  void atomic_clear_bit(unsigned long bit)
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    atomic_mp_and(&this->_bits[idx], ~(1UL << b));
  }

  void clear_all()
  {
    for (unsigned i = 0; i < Nr_elems; ++i)
      this->_bits[i] = 0;
  }

  bool is_empty() const
  {
    for (unsigned i = 0; i < Nr_elems; ++i)
      if (this->_bits[i])
	return false;
    return true;
  }

  void atomic_or(Bitmap_base const &r)
  {
    for (unsigned i = 0; i < Nr_elems; ++i)
      atomic_mp_or(&this->_bits[i], r._bits[i]);
  }

  unsigned ffs(unsigned bit) const
  {
    unsigned long idx = bit / Bpl;
    unsigned long b   = bit % Bpl;
    for (unsigned i = idx; i < Nr_elems; ++i)
      {
        unsigned long v = this->_bits[i];
        v >>= b;
        unsigned r = __builtin_ffsl(v);
        if (r > 0)
          return r + (i * Bpl) + b;

        b = 0;
      }

    return 0;
  }

protected:
  template< bool BIG, unsigned BTS > friend class Bitmap_base;

  Bitmap_base() {}
  Bitmap_base(Bitmap_base const &) = delete;
  Bitmap_base &operator = (Bitmap_base const &) = delete;


  void _or(Bitmap_base const &r)
  {
    for (unsigned i = 0; i < Nr_elems; ++i)
      this->_bits[i] |= r._bits[i];
  }

  template<unsigned SOURCE_BITS>
  void _copy(Bitmap_base<true, SOURCE_BITS> const &s)
  {
    static_assert(BITS <= SOURCE_BITS,
                  "cannot copy smaller bitmap into bigger one");
    __builtin_memcpy(this->_bits, s._bits, Nr_elems * sizeof(this->_bits[0]));
  }
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

  unsigned ffs(unsigned bit) const
  {
    unsigned long v = _bits;
    v >>= bit;
    unsigned r = __builtin_ffsl(v);
    if (r > 0)
      return r + bit;

    return 0;
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
