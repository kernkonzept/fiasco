#ifndef L4_CXX_UART_imx_H__
#define L4_CXX_UART_imx_H__

#include "uart_base.h"

namespace L4
{
  class Uart_imx : public Uart
  {
  public:
    enum platform_type { Type_imx21, Type_imx35, Type_imx51, Type_imx6 };
    explicit Uart_imx(enum platform_type type)
       : _type(type) {}
    bool startup(Io_register_block const *);
    void shutdown();
    bool enable_rx_irq(bool enable = true);
    bool change_mode(Transfer_mode m, Baud_rate r);
    int get_char(bool blocking = true) const;
    int char_avail() const;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count) const;

  private:
    enum platform_type _type;
  };

  class Uart_imx21 : public Uart_imx
  {
  public:
    Uart_imx21() : Uart_imx(Type_imx21) {}
  };

  class Uart_imx35 : public Uart_imx
  {
  public:
    Uart_imx35() : Uart_imx(Type_imx35) {}
  };

  class Uart_imx51 : public Uart_imx
  {
  public:
    Uart_imx51() : Uart_imx(Type_imx51) {}
  };

  class Uart_imx6 : public Uart_imx
  {
  public:
    Uart_imx6() : Uart_imx(Type_imx6) {}
  };
};

#endif
