#ifndef L4_CXX_UART_PL011_H__
#define L4_CXX_UART_PL011_H__

#include "uart_base.h"

namespace L4
{
  class Uart_sifive : public Uart
  {
  public:
    Uart_sifive(unsigned freq) : _freq(freq), _bufchar(-1) {}
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    bool enable_rx_irq(bool enable) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    int tx_avail() const;
    void wait_tx_done() const {}
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count,
              bool blocking = true) const override;

  private:
    unsigned _freq;
    mutable int _bufchar;
  };
};

#endif
