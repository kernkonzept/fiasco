// vi:ft=cpp

#pragma once

#include "types.h"
#include <cxx/type_traits>

class Mmio_register_block
{
public:
  Mmio_register_block() = default;
  explicit Mmio_register_block(Address base) : _base(base) {}

  /** Read-only register mixin */
  template<typename REG, typename VALUE>
  struct Reg_r
  {
    typedef VALUE Type;
    enum { Bits = sizeof(VALUE) * 8 };

    Type read() const
    {
      return *reinterpret_cast<Type volatile *>
        (static_cast<REG const *>(this)->_r);
    }

    operator Type () const { return read(); }
  };

  /** Write-only register mixin */
  template<typename REG, typename VALUE>
  struct Reg_w
  {
    typedef VALUE Type;
    enum { Bits = sizeof(VALUE) * 8 };

    void write(Type val)
    {
      *reinterpret_cast<Type volatile *>
        (static_cast<REG *>(this)->_r) = val;
    }

    void operator = (Type val) { write(val); }
  };


  /** Read-write register mixin */
  template<typename REG, typename VALUE>
  struct Reg_rw : Reg_r<REG, VALUE>, Reg_w<REG, VALUE>
  {
    typedef VALUE Type;
    enum { Bits = sizeof(VALUE) * 8 };

    using Reg_w<REG, VALUE>::operator =;
    using Reg_r<REG, VALUE>::operator Type;

    Type set(Type bits)
    {
      Type tmp = this->read();
      this->write(tmp | bits);
      return tmp | bits;
    }

    Type clear(Type bits)
    {
      Type tmp = this->read();
      this->write(tmp & ~bits);
      return tmp & ~bits;
    }

    Type modify(Type enable, Type disable)
    {
      Type tmp = this->read();
      this->write((tmp & ~disable) | enable);
      return (tmp & ~disable) | enable;
    }
  };

  /** Register wrapper using `VALUE` as access type */
  template<typename VALUE, bool READ = true, bool WRITE = true>
  class Reg_t;

  /** Read-write register wrapper */
  template<typename VALUE>
  class Reg_t<VALUE, true, true>
  : public Reg_rw<Reg_t<VALUE, true, true>, VALUE>
  {
  public:
    Reg_t() = default;
    using Reg_rw<Reg_t<VALUE, true, true>, VALUE>::operator =;

  protected:
    friend class Mmio_register_block;
    friend struct Reg_rw<Reg_t<VALUE, true, true>, VALUE>;
    friend struct Reg_r<Reg_t<VALUE, true, true>, VALUE>;
    friend struct Reg_w<Reg_t<VALUE, true, true>, VALUE>;
    explicit Reg_t(Address a) : _r(a) {}
    Address _r;
  };

  /** Read-only register wrapper */
  template<typename VALUE>
  class Reg_t<VALUE, true, false>
  : public Reg_r<Reg_t<VALUE, true, false>, VALUE>
  {
  public:
    Reg_t() = default;

  protected:
    friend class Mmio_register_block;
    friend class Reg_r<Reg_t<VALUE, true, false>, VALUE>;
    explicit Reg_t(Address a) : _r(a) {}
    Address _r;
  };

  /** Write-only register wrapper */
  template<typename VALUE>
  class Reg_t<VALUE, false, true>
  : public Reg_w<Reg_t<VALUE, false, true>, VALUE>
  {
  public:
    Reg_t() = default;
    using Reg_w<Reg_t<VALUE, false, true>, VALUE>::operator =;

  protected:
    friend class Mmio_register_block;
    friend class Reg_w<Reg_t<VALUE, false, true>, VALUE>;
    explicit Reg_t(Address a) : _r(a) {}
    Address _r;
  };

  /** Register wrapper with `BITS` default access size */
  template<unsigned BITS, bool READ = true, bool WRITE = true>
  struct Reg
  : Reg_t<typename cxx::int_type_for_size<BITS / 8>::type, READ, WRITE>
  {
    typedef typename cxx::int_type_for_size<BITS / 8>::type Type;
    Reg() = default;
    using Reg_t<Type, READ, WRITE>::operator =;

  protected:
    friend class Mmio_register_block;
    explicit Reg(Address a) : Reg_t<Type, READ, WRITE>(a)
    {
      static_assert(BITS % 8 == 0,
                    "Reg<width>:: width must be a multiple of 8");
      static_assert((BITS & (BITS - 1)) == 0,
                    "Reg<width>: width must be a power of 2");
    }
  };

  /** Access register at `offset` with `BITS` width. */
  template<unsigned BITS>
  Reg<BITS> r(Address offset) const
  { return Reg<BITS>(_base + offset); }

  /** Access register at `offset` with access type `T`. */
  template<typename T>
  Reg_t<T> r(Address offset) const
  { return Reg_t<T>(_base + offset); }

  /** Deprecated write to register: use `r<>()` */
  template< typename T >
  void write(T t, Address reg) const
  { r<T>(reg) = t; }

  /** Deprecated read to register: use `r<>()` */
  template< typename T >
  T read(Address reg) const
  { return r<T>(reg); }

  /** Deprecated read to register: use `r<>().modify()` */
  template< typename T >
  void modify(T enable, T disable, Address reg) const
  { r<T>(reg).modify(enable, disable); }

  /** Deprecated read to register: use `r<>().clear()` */
  template< typename T >
  void clear(T clear_bits, Address reg) const
  { r<T>(reg).clear(clear_bits); }

  /** Deprecated read to register: use `r<>().set()` */
  template< typename T >
  void set(T set_bits, Address reg) const
  { r<T>(reg).set(set_bits); }

  Address get_mmio_base() const { return _base; }
  void set_mmio_base(Address base) { _base = base; }

private:
  Address _base;
};

/**
 * Meta function to define the access width in bits for type `REGS`
 * as index type for Register_block.
 * \tparam REGS  The type used as index type for
 *               Register_block<WIDTH, void>::operator [] ()
 *
 * This template must be specialzed for a specific index type used in
 * Register_block<WIDTH, void>::operator [] (REGS idx) and provide the
 * register access width as `value' in bits.
 *
 * For example:
 *     template<> struct Register_block_access_size<Reg_16bit>
 *     { enum { value = 16 }; };
 */
template<typename REGS> struct Register_block_access_size;

/**
 * Typed MMIO register block.
 * \tparam REG_WIDTH  The default register width in bits.
 * \tparam OFFSET     The offset type used for accessing individual
 *                    registers.
 */
template<unsigned REG_WIDTH, typename OFFSET = unsigned>
struct Register_block : Mmio_register_block
{
  typedef OFFSET offset_type;
  enum { Default_width = REG_WIDTH };

  Register_block() = default;
  explicit Register_block(Address base) : Mmio_register_block(base) {}

  /** Access register with default width at `offset`. */
  Mmio_register_block::Reg<Default_width>
  operator [] (offset_type offset) const
  { return this->template r<Default_width>((Address)offset); }

  template<typename REG>
  Mmio_register_block::Reg<Register_block_access_size<REG>::value>
  operator [] (REG reg_offset) const
  { return this->template r<Register_block_access_size<REG>::value>((Address)reg_offset); }
};

/**
 * Typed MMIO register block.
 * \tparam REG_WIDTH  The default register width in bits.
 */
template<unsigned REG_WIDTH>
struct Register_block<REG_WIDTH, void> : Mmio_register_block
{
  enum { Default_width = REG_WIDTH };

  Register_block() = default;
  explicit Register_block(Address base) : Mmio_register_block(base) {}

  template<typename REG>
  Mmio_register_block::Reg<Register_block_access_size<REG>::value>
  operator [] (REG reg_offset) const
  { return this->template r<Register_block_access_size<REG>::value>((Address)reg_offset); }
};
