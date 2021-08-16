#pragma once

#include "uart_base.h"

namespace L4
{
  class Uart_sbi : public Uart
  {
  public:
    Uart_sbi(unsigned);
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    int write(char const *s, unsigned long count) const override;

  private:
    mutable int _bufchar;
  };
};
