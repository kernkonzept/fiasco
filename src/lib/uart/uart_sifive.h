/*
 * Copyright (C) 2021-2024 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "uart_base.h"

namespace L4 {

class Uart_sifive : public Uart
{
public:
  Uart_sifive(unsigned freq) : _freq(freq), _bufchar(-1) {}
  bool startup(Io_register_block const *) override;
  void shutdown() override;
  bool change_mode(Transfer_mode m, Baud_rate r) override;
  int tx_avail() const;
  void wait_tx_done() const;
  inline void out_char(char c) const;
  int write(char const *s, unsigned long count,
            bool blocking = true) const override;

  bool enable_rx_irq(bool enable) override;
  int char_avail() const override;
  int get_char(bool blocking = true) const override;

private:
  unsigned _freq;
  mutable int _bufchar;
};

} // namespace L4
