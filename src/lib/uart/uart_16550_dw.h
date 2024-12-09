/*
 * Copyright (C) 2015 Technische Universität Dresden.
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
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

    void irq_ack() override;
  };
}
