#ifndef L4_CXX_UART_OF_H__
#define L4_CXX_UART_OF_H__

#include "uart_base.h"
#include <stdarg.h>
#include <string.h>
#include "of1275.h"

namespace L4
{
  class Uart_of : public Uart, public Of
  {
  private:
    ihandle_t  _serial;

  public:
    explicit Uart_of(unsigned /*base_rate*/)
      : Of(), _serial(0) {}
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    void out_char(char c) const;
    int write(char const *s, unsigned long count) const override;
  };
};

#endif
