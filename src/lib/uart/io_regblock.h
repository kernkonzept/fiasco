#pragma once

#ifndef __GXX_EXPERIMENTAL_CXX0X__
#ifndef static_assert
#define static_assert(x, y) \
  do { (void)sizeof(char[-(!(x))]); } while (0)
#endif
#endif

namespace L4
{
  class Io_register_block
  {
  public:
    /**
     * \brief Read register with a byte access.
     */
    virtual unsigned char  read8(unsigned long reg) const = 0;

    /**
     * \brief Read register with a 2 byte access.
     */
    virtual unsigned short read16(unsigned long reg) const = 0;

    /**
     * \brief Read register with a 4 byte access.
     */
    virtual unsigned int   read32(unsigned long reg) const = 0;

    /*
     * \brief Read register with an 8 byte access.
     */
    //virtual unsigned long long read64(unsigned long reg) const = 0;

    /**
     * \brief Write register with a byte access.
     */
    virtual void write8(unsigned long reg, unsigned char value) const = 0;

    /**
     * \brief Write register with a 2 byte access.
     */
    virtual void write16(unsigned long reg, unsigned short value) const = 0;

    /**
     * \brief Write register with a 4 byte access.
     */
    virtual void write32(unsigned long reg, unsigned int value) const = 0;

    /*
     * \brief Write register with an 8 byte access.
     */
    //virtual void write64(unsigned long reg, unsigned long long value) const = 0;

    /**
     * \brief Get address of a register.
     */
    //virtual unsigned long addr(unsigned long reg) const = 0;

    /**
     * \brief A delay.
     *
     * Note, most likely this function does nothing.
     */
    virtual void delay() const = 0;

    virtual ~Io_register_block() = 0;

    /**
     * \brief Read register.
     * \param reg   Register to use.
     * \return Value in the register
     *
     * The access width is defined by the return type.
     */
    template< typename R >
    R read(unsigned long reg) const
    {
      switch (sizeof(R))
        {
        case 1: return read8(reg);
        case 2: return read16(reg);
        case 4: return read32(reg);
        default: static_assert(sizeof(R) == 1 || sizeof(R) == 2 || sizeof(R) == 4,
                               "Invalid size");
        };
    }

    /**
     * \brief Write register.
     * \param reg   Register to use.
     * \param value Date to write to the register.
     *
     * The access width is defined by the type of the value
     */
    template< typename R >
    void write(unsigned long reg, R value) const
    {
      switch (sizeof(R))
        {
        case 1: write8(reg, value); return;
        case 2: write16(reg, value); return;
        case 4: write32(reg, value); return;
        default: static_assert(sizeof(R) == 1 || sizeof(R) == 2 || sizeof(R) == 4,
                               "Invalid size");
        };
    }

    /**
     * \brief Modify register.
     * \param reg         Register to use.
     * \param clear_bits  Bits to clear in the register.
     * \param set_bits    Bits to set in the register.
     * \return The written value.
     *
     * This function is a short form of
     * write(reg, (read(reg) & ~clear_bits) | add_bits)
     */
    template< typename R >
    R modify(unsigned long reg, R clear_bits, R set_bits) const
    {
      R r = (read<R>(reg) & ~clear_bits) | set_bits;
      write(reg, r);
      return r;
    }

    /**
     * \brief Set bits in register.
     * \param reg         Register to use.
     * \param set_bits    Bits to set in the registers.
     * \return The written value.
     */
    template< typename R >
    R set(unsigned long reg, R set_bits) const
    {
      return modify<R>(reg, 0, set_bits);
    }

    /**
     * \brief Clear bits in register.
     * \param reg         Register to use.
     * \param clear_bits  Bits to clear in the register.
     * \return The written value.
     */
    template< typename R >
    R clear(unsigned long reg, R clear_bits) const
    {
      return modify<R>(reg, clear_bits, 0);
    }

  };

  inline Io_register_block::~Io_register_block() {}


  class Io_register_block_mmio : public Io_register_block
  {
  private:
    template< typename R >
    R _read(unsigned long reg) const
    { return *reinterpret_cast<volatile R *>(_base + (reg << _shift)); }

    template< typename R >
    void _write(unsigned long reg, R val) const
    { *reinterpret_cast<volatile R *>(_base + (reg << _shift)) = val; }

  public:
    Io_register_block_mmio(unsigned long base, unsigned char shift = 0)
    : _base(base), _shift(shift)
    {}

    unsigned long addr(unsigned long reg) const
    { return _base + (reg << _shift); }

    unsigned char  read8(unsigned long reg) const
    { return _read<unsigned char>(reg); }
    unsigned short read16(unsigned long reg) const
    { return _read<unsigned short>(reg); }
    unsigned int   read32(unsigned long reg) const
    { return _read<unsigned int>(reg); }

    void write8(unsigned long reg, unsigned char val) const
    { _write(reg, val); }
    void write16(unsigned long reg, unsigned short val) const
    { _write(reg, val); }
    void write32(unsigned long reg, unsigned int val) const
    { _write(reg, val); }

    void delay() const
    {}

  private:
    unsigned long _base;
    unsigned char _shift;
  };

  template<typename ACCESS_TYPE>
  class Io_register_block_mmio_fixed_width : public Io_register_block
  {
  private:
    template< typename R >
    R _read(unsigned long reg) const
    { return *reinterpret_cast<volatile ACCESS_TYPE *>(_base + (reg << _shift)); }

    template< typename R >
    void _write(unsigned long reg, R val) const
    { *reinterpret_cast<volatile ACCESS_TYPE *>(_base + (reg << _shift)) = val; }

  public:
    Io_register_block_mmio_fixed_width(unsigned long base, unsigned char shift = 0)
    : _base(base), _shift(shift)
    {}

    unsigned long addr(unsigned long reg) const
    { return _base + (reg << _shift); }

    unsigned char  read8(unsigned long reg) const
    { return _read<unsigned char>(reg); }
    unsigned short read16(unsigned long reg) const
    { return _read<unsigned short>(reg); }
    unsigned int   read32(unsigned long reg) const
    { return _read<unsigned int>(reg); }

    void write8(unsigned long reg, unsigned char val) const
    { _write(reg, val); }
    void write16(unsigned long reg, unsigned short val) const
    { _write(reg, val); }
    void write32(unsigned long reg, unsigned int val) const
    { _write(reg, val); }

    void delay() const
    {}

  private:
    unsigned long _base;
    unsigned char _shift;
  };
}
