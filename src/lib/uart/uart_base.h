#pragma once

#include <stddef.h>
#include "io_regblock.h"

namespace L4
{
  class Uart
  {
  protected:
    unsigned _mode;
    unsigned _rate;
    Io_register_block const *_regs;

  public:
    void *operator new (size_t, void* a)
    { return a; }

  public:
    typedef unsigned Transfer_mode;
    typedef unsigned Baud_rate;

    Uart()
    : _mode(~0U), _rate(~0U)
    {}

    virtual bool startup(Io_register_block const *regs) = 0;

    virtual ~Uart() {}
    virtual void shutdown() = 0;
    virtual bool change_mode(Transfer_mode m, Baud_rate r) = 0;
    virtual int get_char(bool blocking = true) const = 0;
    virtual int char_avail() const = 0;
    virtual int write(char const *s, unsigned long count) const = 0;

    virtual void irq_ack() {}

    virtual bool enable_rx_irq(bool = true) { return false; }
    Transfer_mode mode() const { return _mode; }
    Baud_rate rate() const { return _rate; }
  };
}
