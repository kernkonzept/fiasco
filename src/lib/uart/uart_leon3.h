#ifndef L4_CXX_UART_LEON3_H__
#define L4_CXX_UART_LEON3_H__

#include "uart_base.h"

namespace L4
{
  class Uart_leon3 : public Uart
  {
  public:
    explicit Uart_leon3(unsigned /*base_rate*/) {}
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    bool enable_rx_irq(bool = true) override;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count) const override;
  };
};

#endif
