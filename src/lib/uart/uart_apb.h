/*
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include "uart_base.h"

namespace L4
{
  /**
   * Driver for the Advanced Peripheral Bus (APB) UART from the Cortex-M System
   * Design Kit (CMSDK).
   */
  class Uart_apb : public Uart
  {
  public:
    /** freq == 0 means unknown and don't change baud rate */
    Uart_apb(unsigned freq) : _freq(freq) {}
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    bool enable_rx_irq(bool enable) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    int tx_avail() const;
    void wait_tx_done() const;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count,
              bool blocking = true) const override;

  private:
    void set_rate(Baud_rate r);
    unsigned _freq;
  };
};
