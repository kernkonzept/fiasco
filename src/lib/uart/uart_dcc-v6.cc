/*
 * Copyright (C) 2009 Technische Universität Dresden.
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "uart_dcc-v6.h"

namespace {

enum
{
  DCC_STATUS_TX = 0x20000000, // DTRTX full
  DCC_STATUS_RX = 0x40000000, // DTRRX full
};

}

namespace L4
{
  bool Uart_dcc_v6::startup(Io_register_block const *)
  { return true; }

  void Uart_dcc_v6::shutdown()
  {}

  bool Uart_dcc_v6::change_mode(Transfer_mode, Baud_rate)
  { return true; }

  unsigned Uart_dcc_v6::get_status() const
  {
#ifdef __arm__
    unsigned c;
    asm volatile("mrc p14, 0, %0, c0, c1, 0": "=r" (c));
    return c;
#else
    return 0;
#endif
  }

  int Uart_dcc_v6::get_char(bool blocking) const
  {
#ifdef __arm__
    while (!char_avail())
      if (!blocking)
        return -1;

    int c;
    asm volatile("mrc p14, 0, %0, c0, c5, 0": "=r" (c));
    return c & 0xff;
#else
    static_cast<void>(blocking);
    return -1;
#endif
  }

  int Uart_dcc_v6::char_avail() const
  {
#ifdef __arm__
    return get_status() & DCC_STATUS_RX;
#else
    return false;
#endif
  }

  int Uart_dcc_v6::tx_avail() const
   {
#ifdef __arm__
    return !(get_status() & DCC_STATUS_TX);
#else
    return true;
#endif
  }

  void Uart_dcc_v6::wait_tx_done() const
  {
#ifdef __arm__
    // The DCC interface allows only to check if the TX queue is full.
    while (get_status() & DCC_STATUS_TX)
      ;
#endif
  }

  void Uart_dcc_v6::out_char(char c) const
  {
#ifdef __arm__
    asm volatile("mcr p14, 0, %0, c0, c5, 0": : "r" (c & 0xff));
#else
    (void)c;
#endif
  }

  int Uart_dcc_v6::write(char const *s, unsigned long count,
                         bool blocking) const
  {
#ifdef __arm__
    return generic_write<Uart_dcc_v6, false>(s, count, blocking);
#else
    (void)s;
    (void)count;
    (void)blocking;
    return count;
#endif
  }

};
