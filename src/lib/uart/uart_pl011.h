#ifndef L4_CXX_UART_PL011_H__
#define L4_CXX_UART_PL011_H__

#include "uart_base.h"

namespace L4
{
  class Uart_pl011 : public Uart
  {
  public:
    /** freq == 0 means unknown and don't change baud rate */
    Uart_pl011(unsigned freq) : _freq(freq) {}
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    bool enable_rx_irq(bool enable) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count) const override;

  private:
    void set_rate(Baud_rate r);
    unsigned _freq;
  };
};

#endif
