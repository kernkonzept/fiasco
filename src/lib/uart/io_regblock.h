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
    virtual unsigned char  read8(unsigned long reg) const = 0;
    virtual unsigned short read16(unsigned long reg) const = 0;
    virtual unsigned int   read32(unsigned long reg) const = 0;
    //virtual unsigned long long read64(unsigned long reg) const = 0;

    virtual void write8(unsigned long reg, unsigned char val) const = 0;
    virtual void write16(unsigned long reg, unsigned short val) const = 0;
    virtual void write32(unsigned long reg, unsigned int val) const = 0;
    //virtual void write64(unsigned long reg, unsigned long long val) const = 0;

    virtual void delay() const = 0;

    virtual ~Io_register_block() = 0;

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

    template< typename R >
    void write(unsigned long reg, R val) const
    {
      switch (sizeof(R))
        {
        case 1: write8(reg, val); return;
        case 2: write16(reg, val); return;
        case 4: write32(reg, val); return;
        default: static_assert(sizeof(R) == 1 || sizeof(R) == 2 || sizeof(R) == 4,
                               "Invalid size");
        };
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
