INTERFACE:

#include "types.h"

class Pci
{
  enum
  {
    Cfg_addr_port = 0xcf8,
    Cfg_data_port = 0xcfc,
  };

public:
  enum class Cfg_width
  {
    Byte  = 0,
    Short = 1,
    Long  = 2
  };


  class Cfg_addr
  {
  public:
    Cfg_addr() = default;
    explicit Cfg_addr(Unsigned32 pci_express_adr) : _addr(pci_express_adr) {}

    Cfg_addr(unsigned bus, unsigned dev, unsigned function = 0, unsigned reg = 0)
    : _addr(  ((Unsigned32)bus << 20)
            | ((Unsigned32)dev << 15)
            | ((Unsigned32)function << 12)
            | ((Unsigned32)reg))
    {}


    Cfg_addr operator + (unsigned reg) const
    { return Cfg_addr(_addr + reg); }

    Unsigned32 compat_addr() const
    { return (_addr & 0xfc) | ((_addr >> 4) & 0xffff00); }

    Unsigned32 mmio_cfg_offset() const
    { return _addr; }

    Unsigned32 express_cfg() const
    { return _addr; }

    unsigned reg_offs(Cfg_width w = Cfg_width::Byte) const
    { return (_addr & 0x3) & (~0U << (unsigned)w); }

    unsigned bus() const { return (_addr >> 20) & 0xff; }
    unsigned dev() const { return (_addr >> 15) & 0x1f; }
    unsigned func() const { return (_addr >> 12) & 0x7; }
    unsigned reg() const { return _addr & 0xfff; }

    void bus(unsigned bus) { _addr = (_addr & ~0xff00000) | ((Unsigned32)bus << 20); }
    void dev(unsigned dev) { _addr = (_addr & ~(0x1fUL << 15)) | ((Unsigned32)dev << 15); }
    void func(unsigned func) { _addr = (_addr & ~(0x7UL << 12)) | ((Unsigned32)func << 12); }
    void reg(unsigned reg) { _addr = (_addr & ~0xfff) | (Unsigned32)reg; }

  private:
    Unsigned32 _addr;
  };

};

IMPLEMENTATION:

#include "io.h"
#include <cassert>

PUBLIC static inline NEEDS["io.h", <cassert>]
Unsigned32
Pci::read_cfg(Cfg_addr addr, Cfg_width w)
{
  Io::out32(addr.compat_addr() | (1<<31), Cfg_addr_port);
  switch (w)
    {
    case Cfg_width::Byte:  return Io::in8(Cfg_data_port + addr.reg_offs(w));
    case Cfg_width::Short: return Io::in16(Cfg_data_port + addr.reg_offs(w));
    case Cfg_width::Long:  return Io::in32(Cfg_data_port);
    default: assert (false /* invalid PCI config access */);
    }
  return 0;
}

PUBLIC static template< typename VT > inline
void
Pci::read_cfg(Cfg_addr addr, VT *value)
{
  static_assert (sizeof (VT) == 1 || sizeof (VT) == 2 || sizeof (VT)==4,
                 "wrong PCI config space access size");
  Cfg_width w;
  Io::out32(addr.compat_addr() | (1<<31), Cfg_addr_port);
  switch (sizeof(VT))
    {
    case 1: w = Cfg_width::Byte; break;
    case 2: w = Cfg_width::Short; break;
    case 4: w = Cfg_width::Long; break;
    }

  *value = read_cfg(addr, w);
}


PUBLIC static template< typename VT > inline NEEDS["io.h"]
void
Pci::write_cfg(Cfg_addr addr, VT value)
{
  static_assert (sizeof (VT) == 1 || sizeof (VT) == 2 || sizeof (VT)==4,
                 "wrong PCI config space access size");
  Io::out32(addr.compat_addr() | (1<<31), Cfg_addr_port);
  switch (sizeof(VT))
    {
    case 1: Io::out8(value, Cfg_data_port + addr.reg_offs(Cfg_width::Byte)); break;
    case 2: Io::out16(value, Cfg_data_port + addr.reg_offs(Cfg_width::Short)); break;
    case 4: Io::out32(value, Cfg_data_port + addr.reg_offs(Cfg_width::Long)); break;
    }
}

