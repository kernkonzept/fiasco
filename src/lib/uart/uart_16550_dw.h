/*
 * (c) 2015 Adam Lackorzynski <adam@l4re.org>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "uart_16550.h"

namespace L4
{
  class Uart_16550_dw : public Uart_16550
  {
  public:
    explicit Uart_16550_dw(unsigned long base_rate)
    : Uart_16550(base_rate)
    {}

    void irq_ack();
  };
}
