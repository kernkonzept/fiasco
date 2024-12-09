/*
 * Copyright (C) 2019, 2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "uart_base.h"

namespace L4
{
  class Uart_lpuart : public Uart
  {
  public:
    /** freq == 0 means unknown and don't change baud rate */
    Uart_lpuart(unsigned freq = 0) : _freq(freq) {}
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool enable_rx_irq(bool enable = true) override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    int tx_avail() const;
    void wait_tx_done() const {}
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count,
              bool blocking = true) const override;

  private:
    unsigned _freq;
  };
};
