#include "uart_sbi.h"

namespace
{
  class Sbi
  {
  public:
    static inline void console_putchar(int ch)
    {
      sbi_call(Sbi_console_putchar, ch);
    }

    static inline int console_getchar()
    {
      return sbi_call(Sbi_console_getchar);
    }

  private:
    enum
    {
      Sbi_console_putchar = 1,
      Sbi_console_getchar = 2,
    };

    static inline unsigned long sbi_call(unsigned long ext_id,
                                         unsigned long arg0 = 0)
    {
      register unsigned long a0 asm("a0") = arg0;
      register unsigned long a7 asm("a7") = ext_id;
      __asm__ __volatile__ ("ecall" : "+r"(a0) : "r"(a7) : "memory");
      return a0;
    }
  };
}

namespace L4
{
  Uart_sbi::Uart_sbi(unsigned) : _bufchar(-1)
  {}

  bool Uart_sbi::startup(Io_register_block const *)
  {
    return true;
  }

  void Uart_sbi::shutdown()
  {}

  bool Uart_sbi::change_mode(Transfer_mode, Baud_rate)
  {
    return true;
  }

  int Uart_sbi::get_char(bool blocking) const
  {
    while (!char_avail())
     if (!blocking)
       return -1;

    int c = _bufchar;
    _bufchar = -1;
    return c;
  }

  int Uart_sbi::char_avail() const
  {
    if (_bufchar == -1)
      _bufchar = Sbi::console_getchar();
    return _bufchar != -1;
  }

  int Uart_sbi::write(char const *s, unsigned long count) const
  {
    unsigned long c = count;
    while (c--)
      Sbi::console_putchar(*s++);

    return count;
  }
};
