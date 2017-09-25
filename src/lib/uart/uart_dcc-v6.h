#ifndef L4_CXX_UART_DCC_V6_H__
#define L4_CXX_UART_DCC_V6_H__

#include "uart_base.h"

namespace L4
{
  class Uart_dcc_v6 : public Uart
  {
  public:
    bool startup(Io_register_block const *);
    void shutdown();
    bool change_mode(Transfer_mode m, Baud_rate r);
    int get_char(bool blocking = true) const;
    int char_avail() const;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count) const;
  };
};

#endif
