/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Stephan Gerhold <stephan.gerhold@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "uart_base.h"

namespace L4 {

class Uart_geni : public Uart
{
public:
  explicit Uart_geni(unsigned /*base_rate*/) {}
  bool startup(Io_register_block const *) override;
  void shutdown() override;
  bool change_mode(Transfer_mode m, Baud_rate r) override;
  int tx_avail() const;
  void wait_tx_done() const;
  void out_char(char c) const;
  int write(char const *s, unsigned long count,
            bool blocking = true) const override;

  bool enable_rx_irq(bool enable = true) override;
  void irq_ack() override;
  int char_avail() const override;
  int get_char(bool blocking = true) const override;
};

} // namespace L4
