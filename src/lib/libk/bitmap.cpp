INTERFACE:

/**
 * \file
 *
 * Reuseable bitmap implementation.
 *
 * \note The bitmap implementation generally does not do any range checking.
 *       Therefore the responsibility of avoiding out-of-bound memory accesses
 *       lies on the user of this code.
 */

#include "atomic.h"
#include <cxx/type_traits>

template<bool LARGE, size_t BITS>
class Bitmap_base;

/**
 * Wrapper implementing basic bitmap operations on top of a storage type.
 *
 * \note The actual data of the bitmap is not explicitly initialized by this
 *       wrapper prior to use. If any such initialization is required, the
 *       storage type either needs to be a scalar type or define an
 *       initializing constructor.
 *
 * \tparam STORAGE_TYPE  Data type that contains the actual data of the bitmap.
 *                       This is usually a pointer to an unsigned integer type,
 *                       an array of an unsigned integer type or a different
 *                       compatible type for which bitwise operators and the
 *                       index operator are defined.
 */
template<typename STORAGE_TYPE>
class Bitmap_storage
{
public:
  /**
   * Type of the individual bitmap element.
   */
  using Bitmap_elem_type
    = cxx::remove_pointer_t<cxx::remove_all_extents_t<STORAGE_TYPE>>;

  enum : size_t
  {
    /**
     * Number of bits per bitmap element.
     */
    Bpl = sizeof(Bitmap_elem_type) * 8
  };

  /**
   * Get the number of bitmap elements that hold the given number of bits.
   *
   * \param nr_bits  Number of bits to represent.
   *
   * \return Number of bitmap elements that hold \ref nr_bits bits.
   */
  static constexpr size_t
  nr_elems(size_t nr_bits)
  {
    return (nr_bits + Bpl - 1) / Bpl;
  }

  /**
   * Get the size (in bytes) of a bitmap that holds the given number of bits.
   *
   * \param nr_bits  Number of bits to represent.
   *
   * \return Size (in bytes) of a bitmap that holds \ref nr_bits bits.
   */
  static constexpr size_t
  size_in_bytes(size_t nr_bits)
  {
    return nr_elems(nr_bits) * sizeof(Bitmap_elem_type);
  }

  /**
   * Assign a bit in the bitmap.
   *
   * \param bit  Bit index to assign.
   * \param on   Value to assign (false/true).
   */
  void bit(size_t bit, bool on)
  {
    size_t idx = bit / Bpl;
    size_t pos = bit % Bpl;

    Bitmap_elem_type mask = static_cast<Bitmap_elem_type>(1U) << pos;
    Bitmap_elem_type val = static_cast<Bitmap_elem_type>(on) << pos;

    this->_bits[idx] &= ~mask;
    this->_bits[idx] |= val;
  }

  /**
   * Clear a bit in the bitmap.
   *
   * \param bit  Bit index to clear.
   */
  void clear_bit(size_t bit)
  {
    size_t idx = bit / Bpl;
    size_t pos = bit % Bpl;

    Bitmap_elem_type mask = static_cast<Bitmap_elem_type>(1U) << pos;

    this->_bits[idx] &= ~mask;
  }

  /**
   * Set a bit in the bitmap.
   *
   * \param bit  Bit index to set.
   */
  void set_bit(size_t bit)
  {
    size_t idx = bit / Bpl;
    size_t pos = bit % Bpl;

    Bitmap_elem_type mask = static_cast<Bitmap_elem_type>(1U) << pos;

    this->_bits[idx] |= mask;
  }

  /**
   * Access a bit in the bitmap.
   *
   * \param bit  Bit index to access.
   *
   * \retval False if the given bit is cleared.
   * \retval True if the given bit is set.
   */
  bool operator [](size_t bit) const
  {
    size_t idx = bit / Bpl;
    size_t pos = bit % Bpl;

    Bitmap_elem_type mask = static_cast<Bitmap_elem_type>(1U) << pos;

    return !!(this->_bits[idx] & mask);
  }

protected:
  /**
   * Actual bitmap data.
   */
  STORAGE_TYPE _bits;
};

/**
 * Bitmap storing a certain number of bits.
 *
 * The implementation uses an array of unsigned long as the storage type for
 * the bitmap. Thus the actual memory size used by the implementation is
 * rounded up to an integer multiple of the unsigned long size.
 *
 * \note The bitmap is not explicitly initialized prior to use.
 *
 * \tparam BITS  Number of bits to store in the bitmap.
 */
template<size_t BITS>
struct Bitmap_storage_size :
  Bitmap_storage<unsigned long[(BITS + sizeof(unsigned long) * 8 - 1)
                               / (sizeof(unsigned long) * 8)]>
{
};

/**
 * Bitmap storing more than sizeof(unsigned long) * 8 bits.
 *
 * \tparam BITS  Number of bits to store in the bitmap.
 */
template<size_t BITS>
class Bitmap_base<true, BITS> : public Bitmap_storage_size<BITS>
{
public:
  /**
   * Type of the individual bitmap element.
   */
  using Bitmap_elem_type = typename Bitmap_storage_size<BITS>::Bitmap_elem_type;

  enum : size_t
  {
    /**
     * Number of bits per bitmap element.
     */
    Bpl = Bitmap_storage_size<BITS>::Bpl,

    /**
     * Number of bitmap elements.
     */
    Nr_elems = Bitmap_storage_size<BITS>::nr_elems(BITS),

    /**
     * Size of the bitmap in bytes.
     */
    Size_in_bytes = Bitmap_storage_size<BITS>::size_in_bytes(BITS),
  };

  /**
   * Atomically access and clear a bit in the bitmap.
   *
   * There is a non-atomic fast path that potentially skips the write to the
   * bitmap if the bit is already cleared.
   *
   * \param bit  Bit index to access and clear.
   *
   * \retval False if the given bit was originally cleared.
   * \retval True if the given bit was originally set.
   */
  bool atomic_get_and_clear(size_t bit)
  {
    static_assert(Bpl == sizeof(unsigned long) * 8,
                  "Invalid type of Bitmap_storage_size");

    size_t idx = bit / Bpl;
    size_t pos = bit % Bpl;

    Bitmap_elem_type mask = static_cast<Bitmap_elem_type>(1U) << pos;

    if (!(this->_bits[idx] & mask))
      return false;

    Bitmap_elem_type elem;

    do
      {
        elem = this->_bits[idx];
      }
    while (!cas(&this->_bits[idx], elem, elem & ~mask));

    return !!(elem & mask);
  }

  /**
   * Atomically access and clear a bit in the bitmap.
   *
   * Contrary to \ref atomic_get_and_clear(), the bitmap is never written if
   * the bit is already cleared (and the return value is false).
   *
   * \param bit  Bit index to access and clear.
   *
   * \retval False if the given bit was originally cleared.
   * \retval True if the given bit was originally set.
   */
  bool atomic_get_and_clear_if_set(size_t bit)
  {
    size_t idx = bit / Bpl;
    size_t pos = bit % Bpl;

    Bitmap_elem_type mask = static_cast<Bitmap_elem_type>(1U) << pos;
    Bitmap_elem_type elem;

    do
      {
        elem = this->_bits[idx];
        if (!(elem & mask))
          return false;
      }
    while (!cas(&this->_bits[idx], elem, elem & ~mask));

    return true;
  }

  /**
   * Atomically access and set a bit in the bitmap.
   *
   * \param bit  Bit index to access and set.
   *
   * \retval False if the given bit was originally cleared.
   * \retval True if the given bit was originally set.
   */
  bool atomic_get_and_set(size_t bit)
  {
    size_t idx = bit / Bpl;
    size_t pos = bit % Bpl;

    Bitmap_elem_type mask = static_cast<Bitmap_elem_type>(1U) << pos;
    Bitmap_elem_type elem;

    do
      {
        elem = this->_bits[idx];
      }
    while (!cas(&this->_bits[idx], elem, elem | mask));

    return !!(elem & mask);
  }

  /**
   * Atomically access and set a bit in the bitmap.
   *
   * Contrary to \ref atomic_get_and_set(), the bitmap is never written if
   * the bit is already set (and the return value is true).
   *
   * \param bit  Bit index to access and set.
   *
   * \retval False if the given bit was originally cleared.
   * \retval True if the given bit was originally set.
   */
  bool atomic_get_and_set_if_unset(size_t bit)
  {
    size_t idx = bit / Bpl;
    size_t pos = bit % Bpl;

    Bitmap_elem_type mask = static_cast<Bitmap_elem_type>(1U) << pos;
    Bitmap_elem_type elem;

    do
      {
        elem = this->_bits[idx];
        if (elem & mask)
          return true;
      }
    while (!cas(&this->_bits[idx], elem, elem | mask));

    return false;
  }

  /**
   * Atomically clear a bit in the bitmap.
   *
   * \param bit  Bit index to clear.
   */
  void atomic_clear_bit(size_t bit)
  {
    size_t idx = bit / Bpl;
    size_t pos = bit % Bpl;

    Bitmap_elem_type mask = static_cast<Bitmap_elem_type>(1U) << pos;

    ::atomic_and(&this->_bits[idx], ~mask);
  }

  /**
   * Atomically set a bit in the bitmap.
   *
   * \param bit  Bit index to set.
   */
  void atomic_set_bit(size_t bit)
  {
    size_t idx = bit / Bpl;
    size_t pos = bit % Bpl;

    Bitmap_elem_type mask = static_cast<Bitmap_elem_type>(1U) << pos;

    ::atomic_or(&this->_bits[idx], mask);
  }

  /**
   * Invert all bits.
   */
  void invert()
  {
    for (size_t i = 0; i < Nr_elems; ++i)
      this->_bits[i] = ~this->_bits[i];
  }

  /**
   * Clear all bits in the bitmap.
   *
   * \note The clearing is performed using the memset operation. Therefore the
   *       bits beyond the bit range (but within the byte size range of the
   *       storage type) are also written.
   */
  void clear_all()
  {
    __builtin_memset(this->_bits, 0, Size_in_bytes);
  }

  /**
   * Check whether all bits of the bitmap are cleared.
   *
   * \retval False if at least one bit in the bitmap is set.
   * \retval True if all bits in the bitmap are cleared.
   */
  bool is_empty() const
  {
    for (size_t i = 0; i < Nr_elems; ++i)
      if (this->_bits[i])
        return false;

    return true;
  }

  /**
   * Logically disjoint (or) the bitmap with a different bitmap.
   *
   * \note The operation is atomic with the granularity of the individual
   *       unsigned longs that form the storage type. It is not atomic with
   *       respect to the entire bitmap.
   *
   * \note Since the disjoint is performed on unsigned longs, the bits beyond
   *       the bit range (but within the byte size range of the storage type)
   *       are also read/written.
   *
   * \param r  Bitmap to logically disjoint the bitmap with.
   */
  void atomic_or(Bitmap_base const &r)
  {
    for (size_t i = 0; i < Nr_elems; ++i)
      if (r._bits[i])
        ::atomic_or(&this->_bits[i], r._bits[i]);
  }

  /**
   * Get 1 plus the index of the least significant set bit in the bitmap
   * ("find first set"), starting at the given bit index.
   *
   * \param bit  Bit index to start search at (inclusive).
   *
   * \retval 0 if the bitmap (starting from the given bit index) contains only
   *         cleared bits.
   * \return 1 plus the index of the least significant set bit in the bitmap
   *         (starting at the given bit index).
   */
  size_t ffs(size_t bit) const
  {
    size_t idx = bit / Bpl;
    size_t pos = bit % Bpl;

    for (size_t i = idx; i < Nr_elems; ++i)
      {
        Bitmap_elem_type elem = this->_bits[i];
        elem >>= pos;

        unsigned int r = __builtin_ffsl(elem);
        if (r > 0)
          return r + (i * Bpl) + pos;

        pos = 0;
      }

    return 0;
  }

  /**
   * Provide raw access to underlying storage.
   */
  unsigned long const *raw() const
  {
    return this->_bits;
  }

protected:
  template<bool LARGE, size_t BTS> friend class Bitmap_base;

  Bitmap_base()
  {
  }

  Bitmap_base(Bitmap_base const &) = delete;
  Bitmap_base &operator =(Bitmap_base const &) = delete;

  /**
   * Logically disjoint (or) the bitmap with a different bitmap.
   *
   * \note Since the disjoint is performed on unsigned longs, the bits beyond
   *       the bit range (but within the byte size range of the storage type)
   *       are also read/written.
   *
   * \param r  Bitmap to logically disjoint the bitmap with.
   */
  void _or(Bitmap_base const &r)
  {
    for (size_t i = 0; i < Nr_elems; ++i)
      this->_bits[i] |= r._bits[i];
  }

  /**
   * Logically conjoint (and) the bitmap with a different bitmap.
   *
   * \note Since the conjoint is performed on unsigned longs, the bits beyond
   *       the bit range (but within the byte size range of the storage type)
   *       are also read/written.
   *
   * \param r  Bitmap to logically conjoint the bitmap with.
   */
  void _and(Bitmap_base const &r)
  {
    for (size_t i = 0; i < Nr_elems; ++i)
      this->_bits[i] &= r._bits[i];
  }

  /**
   * Overwrite the bitmap with a different bitmap.
   *
   * \note The given bitmap must be at least as large as the bitmap.
   *
   * \note Since the copy is performed using the memcpy operation, the bits
   *       beyond the bit range (but within the byte size range of the storage
   *       type) are also read/written.
   *
   * \param s  Given bitmap to overwrite the bitmap with.
   */
  template<size_t SOURCE_BITS>
  void _copy(Bitmap_base<true, SOURCE_BITS> const &s)
  {
    static_assert(BITS <= SOURCE_BITS,
                  "Cannot copy smaller bitmap into bigger one");

    __builtin_memcpy(this->_bits, s._bits, Size_in_bytes);
  }
};

/**
 * Bitmap storing at most sizeof(unsigned long) * 8 bits.
 *
 * The storage type is a single scalar unsigned long.
 *
 * \tparam BITS  Number of bits to store in the bitmap.
 */
template<size_t BITS>
class Bitmap_base<false, BITS>
{
public:
  /**
   * Assign a bit in the bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param bit  Bit index to assign.
   * \param on   Value to assign (false/true).
   */
  void bit(size_t bit, bool on)
  {
    unsigned long mask = 1UL << bit;
    unsigned long val = static_cast<unsigned long>(on) << bit;

    this->_bits &= ~mask;
    this->_bits |= val;
  }

  /**
   * Clear a bit in the bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param bit  Bit index to clear.
   */
  void clear_bit(size_t bit)
  {
    unsigned long mask = 1UL << bit;

    _bits &= ~mask;
  }

  /**
   * Set a bit in the bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param bit  Bit index to set.
   */
  void set_bit(size_t bit)
  {
    unsigned long mask = 1UL << bit;

    _bits |= mask;
  }

  /**
   * Access a bit in the bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param bit  Bit index to access.
   *
   * \retval False if the given bit is cleared.
   * \retval True if the given bit is set.
   */
  bool operator [](size_t bit) const
  {
    unsigned long mask = 1UL << bit;

    return !!(_bits & mask);
  }

  /**
   * Atomically access and clear a bit in the bitmap.
   *
   * There is a non-atomic fast path that potentially skips the write to the
   * bitmap if the bit is already cleared.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param bit  Bit index to access and clear.
   *
   * \retval False if the given bit was originally cleared.
   * \retval True if the given bit was originally set.
   */
  bool atomic_get_and_clear(size_t bit)
  {
    unsigned long mask = 1UL << bit;

    if (!(_bits & mask))
      return false;

    unsigned long elem;

    do
      {
        elem = _bits;
      }
    while (!cas(&_bits, elem, elem & ~mask));

    return !!(elem & mask);
  }

  /**
   * Atomically access and clear a bit in the bitmap.
   *
   * Contrary to \ref atomic_get_and_clear(), the bitmap is never written if
   * the bit is already cleared (and the return value is false).
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param bit  Bit index to access and clear.
   *
   * \retval False if the given bit was originally cleared.
   * \retval True if the given bit was originally set.
   */
  bool atomic_get_and_clear_if_set(size_t bit)
  {
    unsigned long mask = 1UL << bit;
    unsigned long elem;

    do
      {
        elem = _bits;
        if (!(elem & mask))
          return false;
      }
    while (!cas(&_bits, elem, elem & ~mask));

    return true;
  }

  /**
   * Atomically access and set a bit in the bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param bit  Bit index to access and set.
   *
   * \retval False if the given bit was originally cleared.
   * \retval True if the given bit was originally set.
   */
  bool atomic_get_and_set(size_t bit)
  {
    unsigned long mask = 1UL << bit;
    unsigned long elem;

    do
      {
        elem = _bits;
      }
    while (!cas(&_bits, elem, elem | mask));

    return !!(elem & mask);
  }

  /**
   * Atomically access and set a bit in the bitmap.
   *
   * Contrary to \ref atomic_get_and_set(), the bitmap is never written if
   * the bit is already set (and the return value is true).
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param bit  Bit index to access and set.
   *
   * \retval False if the given bit was originally cleared.
   * \retval True if the given bit was originally set.
   */
  bool atomic_get_and_set_if_unset(size_t bit)
  {
    unsigned long mask = 1UL << bit;
    unsigned long elem;

    do
      {
        elem = _bits;
        if (elem & mask)
          return true;
      }
    while (!cas(&_bits, elem, elem | mask));

    return false;
  }

  /**
   * Atomically clear a bit in the bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param bit  Bit index to clear.
   */
  void atomic_clear_bit(size_t bit)
  {
    unsigned long mask = 1UL << bit;

    ::atomic_and(&_bits, ~mask);
  }

  /**
   * Atomically set a bit in the bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param bit  Bit index to set.
   */
  void atomic_set_bit(size_t bit)
  {
    unsigned long mask = 1UL << bit;

    ::atomic_or(&_bits, mask);
  }

  /**
   * Invert all bits.
   */
  void invert()
  {
    _bits = ~_bits;
  }

  /**
   * Clear all bits in the bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   */
  void clear_all()
  {
    _bits = 0;
  }

  /**
   * Check whether all bits of the bitmap are cleared.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \retval False if at least one bit in the bitmap is set.
   * \retval True if all bits in the bitmap are cleared.
   */
  bool is_empty() const
  {
    return !_bits;
  }

  /**
   * Logically disjoint (or) the bitmap with a different bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \note The operation is atomic except for a fast-path check that tests
   *       whether the given bitmap is all cleared.
   *
   * \param r  Bitmap to logically disjoint the bitmap with.
   */
  void atomic_or(Bitmap_base const &r)
  {
    if (r._bits)
      ::atomic_or(&_bits, r._bits);
  }

  /**
   * Get 1 plus the index of the least significant set bit in the bitmap
   * ("find first set"), starting at the given bit index.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param bit  Bit index to start search at (inclusive).
   *
   * \retval 0 if the bitmap (starting from the given bit index) contains only
   *         cleared bits.
   * \return 1 plus the index of the least significant set bit in the bitmap
   *         (starting at the given bit index).
   */
  size_t ffs(size_t bit) const
  {
    unsigned long elem = _bits;
    elem >>= bit;

    unsigned int r = __builtin_ffsl(elem);
    if (r > 0)
      return r + bit;

    return 0;
  }

  /**
   * Provide raw access to underlying storage.
   */
  unsigned long const *raw() const
  {
    return &_bits;
  }

protected:
  template<bool LARGE, size_t BTS> friend class Bitmap_base;

  enum : size_t
  {
    /**
     * Number of bits per bitmap element.
     */
    Bpl = sizeof(unsigned long) * 8,

    /**
     * Number bitmap elements (1).
     */
    Nr_elems = 1,
  };

  /**
   * Actual bitmap data.
   */
  unsigned long _bits;

  Bitmap_base() = default;
  Bitmap_base(Bitmap_base const &) = delete;
  Bitmap_base &operator =(Bitmap_base const &) = delete;

  /**
   * Logically disjoint (or) the bitmap with a different bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param r  Bitmap to logically disjoint the bitmap with.
   */
  void _or(Bitmap_base const &r)
  {
    _bits |= r._bits;
  }

  /**
   * Logically conjoint (and) the bitmap with a different bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param r  Bitmap to logically conjoint the bitmap with.
   */
  void _and(Bitmap_base const &r)
  {
    _bits &= r._bits;
  }

  void _copy(Bitmap_base const &s)
  {
    _bits = s._bits;
  }

  /**
   * Overwrite the bitmap with a different bitmap.
   *
   * \note The given bitmap must be at least as large as the bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type.
   *
   * \param s  Given bitmap to overwrite the bitmap with.
   */
  template<size_t SOURCE_BITS>
  void _copy(Bitmap_base<false, SOURCE_BITS> const &s)
  {
    static_assert(BITS <= SOURCE_BITS,
                  "Cannot copy smaller bitmap into bigger one");

    _bits = s._bits;
  }

  /**
   * Overwrite the bitmap with a different bitmap.
   *
   * \note The given bitmap must be at least as large as the bitmap.
   *
   * This is an optimized version for the single scalar unsigned long storage
   * type (however, the given bitmap uses the generic array-based storage
   * type).
   *
   * \param s  Given bitmap to overwrite the bitmap with.
   */
  template<size_t SOURCE_BITS>
  void _copy(Bitmap_base<true, SOURCE_BITS> const &s)
  {
    static_assert(BITS <= SOURCE_BITS,
                  "Cannot copy smaller bitmap into bigger one");

    _bits = s._bits[0];
  }

};

/**
 * Bitmap implementation.
 *
 * Depending on the number of bits requested, either the optimized variant with
 * only a single scalar unsigned long as the storage type, or the viarant with
 * the generic array-based storage type is selected.
 *
 * \tparam BITS  Number of bits to store in the bitmap.
 */
template<size_t BITS>
class Bitmap : public Bitmap_base<(BITS > sizeof(unsigned long) * 8), BITS>
{
public:
  Bitmap()
  {
  }

  /**
   * Copy constructor.
   *
   * \param o  Bitmap to copy from.
   */
  Bitmap(Bitmap const &o) : Bitmap_base<(BITS > sizeof(unsigned long) * 8), BITS>()
  {
    this->_copy(o);
  }

  /**
   * Get the number of bits the bitmap can store.
   */
  static constexpr size_t size()
  { return BITS; }

  /**
   * Assignment operator.
   *
   * This variant accepts the source bitmap of the same type.
   *
   * \param o  Bitmap to assign from.
   */
  Bitmap &operator =(Bitmap const &o)
  {
    this->_copy(o);
    return *this;
  }

  /**
   * Assignment operator.
   *
   * Generic variant for any source bitmap that is at least as large as the
   * bitmap.
   *
   * \param o  Bitmap to assign from.
   */
  template<size_t SOURCE_BITS>
  Bitmap &operator =(Bitmap<SOURCE_BITS> const &o)
  {
    this->_copy(o);
    return *this;
  }

  /** Disjoint operator.
   *
   * Logically disjoint (or) the bitmap with a different bitmap.
   *
   * \param o  Bitmap to logically disjoint the bitmap with.
   */
  Bitmap &operator |=(Bitmap const &o)
  {
    this->_or(o);
    return *this;
  }

  /** Conjoint operator.
   *
   * Logically conjoint (and) the bitmap with a different bitmap.
   *
   * \param o  Bitmap to logically conjoint the bitmap with.
   */
  Bitmap &operator &=(Bitmap const &o)
  {
    this->_and(o);
    return *this;
  }
};
