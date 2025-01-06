/*
 * Copyright (C) 2021, 2024 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "uart_sbi.h"

namespace
{
  enum {
    Sbi_console_putchar = 1,
    Sbi_console_getchar = 2,
  };

  inline
  unsigned long _sbi_call(unsigned long call_type, unsigned long arg0 = 0)
  {
    register unsigned long a0 asm("a0") = arg0;
    register unsigned long a7 asm("a7") = call_type;
    __asm__ __volatile__ ("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
  }

  inline
  void sbi_console_putchar(int ch)
  {
    _sbi_call(Sbi_console_putchar, ch);
  }

  inline
  int sbi_console_getchar(void)
  {
    return _sbi_call(Sbi_console_getchar);
  }
}

namespace L4
{
  Uart_sbi::Uart_sbi() : _bufchar(-1)
  {}

  bool Uart_sbi::startup(Io_register_block const *)
  { return true; }

  void Uart_sbi::shutdown()
  {}

  bool Uart_sbi::change_mode(Transfer_mode, Baud_rate)
  { return true; }

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
      _bufchar = sbi_console_getchar();
    return _bufchar != -1;
  }

  void Uart_sbi::out_char(char c) const
  {
    sbi_console_putchar(c);
  }

  int Uart_sbi::write(char const *s, unsigned long count, bool blocking) const
  {
    return generic_write<Uart_sbi>(s, count, blocking);
  }
}
