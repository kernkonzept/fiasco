/*
 * Copyright (C) 2017, 2023-2024 Kernkonzept GmbH.
 * Author(s) Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "uart_base.h"

namespace L4 {

class Uart_mvebu : public Uart
{
public:
  explicit Uart_mvebu(unsigned baserate) : _baserate(baserate) {}
  bool startup(Io_register_block const *) override;
  void shutdown() override;
  bool change_mode(Transfer_mode m, Baud_rate r) override;
  int tx_avail() const;
  void wait_tx_done() const {}
  inline void out_char(char c) const;
  int write(char const *s, unsigned long count,
            bool blocking = true) const override;

  bool enable_rx_irq(bool enable = true) override;
  int char_avail() const override;
  int get_char(bool blocking = true) const override;

private:
  unsigned _baserate;
};

} // namespace L4
