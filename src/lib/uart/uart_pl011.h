/*
 * Copyright (C) 2009 Technische Universität Dresden.
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "uart_base.h"

namespace L4
{
  class Uart_pl011 : public Uart
  {
  public:
    /** freq == 0 means unknown and don't change baud rate */
    Uart_pl011(unsigned freq) : _freq(freq) {}
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
