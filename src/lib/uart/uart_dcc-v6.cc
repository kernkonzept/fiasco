/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 */
/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "uart_dcc-v6.h"
#include "poll_timeout_counter.h"

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
#ifdef ARCH_arm
    unsigned c;
    asm volatile("mrc p14, 0, %0, c0, c1, 0": "=r" (c));
    return c;
#else
    return 0;
#endif
  }

  int Uart_dcc_v6::get_char(bool /*blocking*/) const
  {
#ifdef ARCH_arm
    int c;
    asm volatile("mrc p14, 0, %0, c0, c5, 0": "=r" (c));
    return c & 0xff;
#else
    return 0;
#endif
  }

  int Uart_dcc_v6::char_avail() const
  {
#ifdef ARCH_arm
    return get_status() & DCC_STATUS_RX;
#else
    return false;
#endif
  }

  int Uart_dcc_v6::tx_avail() const
   {
#ifdef ARCH_arm
    return !(get_status() & DCC_STATUS_TX);
#else
    return true;
#endif
  }

  void Uart_dcc_v6::wait_tx_done() const
  {
#ifdef ARCH_arm
    // The DCC interface allows only to check if the TX queue is full.
    Poll_timeout_counter i(100000);
    while (i.test(get_status() & DCC_STATUS_TX))
      ;
#endif
  }

  void Uart_dcc_v6::out_char(char c) const
  {
#ifdef ARCH_arm
    asm volatile("mcr p14, 0, %0, c0, c5, 0": : "r" (c & 0xff));
#else
    (void)c;
#endif
  }

  int Uart_dcc_v6::write(char const *s, unsigned long count,
                         bool blocking) const
  {
#ifdef ARCH_arm
    return generic_write<Uart_dcc_v6>(s, count, blocking);
#else
    (void)s;
    (void)count;
    (void)blocking;
    return count;
#endif
  }

};
