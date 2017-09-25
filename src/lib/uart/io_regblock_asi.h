#pragma once

#include "io_regblock.h"
#include "processor.h"

namespace L4
{
  class Io_register_block_asi : public Io_register_block
  {
  public:
    Io_register_block_asi(unsigned long base)
    : _base(base)
    {}

    unsigned char  read8(unsigned long reg) const
    {
      return Proc::read_alternative<0x1C>(_base + reg) & 0xFF;
    }

    unsigned short read16(unsigned long reg) const
    {
      return Proc::read_alternative<0x1C>(_base + reg) & 0xFFFF;
    }

    unsigned int   read32(unsigned long reg) const
    {
      return Proc::read_alternative<0x1C>(_base + reg);
    }

    void write8(unsigned long reg, unsigned char val) const
    {
      Proc::write_alternative<0x1C>(_base + reg, val);
    }

    void write16(unsigned long reg, unsigned short val) const
    {
      Proc::write_alternative<0x1C>(_base + reg, val);
    }

    void write32(unsigned long reg, unsigned int val) const
    {
      Proc::write_alternative<0x1C>(_base + reg, val);
    }

    void delay() const
    {
    }

  private:
    unsigned long _base;
  };
}
