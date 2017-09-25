#ifndef L4_CXX_UART_PL011_H__
#define L4_CXX_UART_PL011_H__

#include "uart_base.h"

namespace L4
{
  class Uart_pl011 : public Uart
  {
  public:
    Uart_pl011(unsigned freq) : _freq(freq) {}
    bool startup(Io_register_block const *);
    void shutdown();
    bool change_mode(Transfer_mode m, Baud_rate r);
    bool enable_rx_irq(bool enable);
    int get_char(bool blocking = true) const;
    int char_avail() const;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count) const;

  private:
    unsigned _freq;
  };
};

#endif
