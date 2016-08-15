#ifndef L4_CXX_UART_LEON3_H__
#define L4_CXX_UART_LEON3_H__

#include "uart_base.h"

namespace L4
{
  class Uart_leon3 : public Uart
  {
  public:
    bool startup(Io_register_block const *);
    void shutdown();
    bool change_mode(Transfer_mode m, Baud_rate r);
    int get_char(bool blocking = true) const;
    int char_avail() const;
    bool enable_rx_irq(bool = true);
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count) const;
  };
};

#endif
