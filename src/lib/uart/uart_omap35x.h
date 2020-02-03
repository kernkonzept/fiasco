#ifndef L4_CXX_UART_OMAP35X_H__
#define L4_CXX_UART_OMAP35X_H__

#include "uart_base.h"

namespace L4
{
  class Uart_omap35x : public Uart
  {
  public:
    explicit Uart_omap35x(unsigned /*base_rate*/) {}
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    bool enable_rx_irq(bool) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count) const override;
  };
};

#endif
