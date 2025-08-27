/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "uart_base.h"

namespace L4 {

class Uart_tegra_tcu : public Uart
{
public:
  Uart_tegra_tcu() {}
  Uart_tegra_tcu(unsigned long) {}
  bool startup(Io_register_block const *tx_mbox) override;
  void set_rx_mbox(Io_register_block const *rx_mbox);
  void shutdown() override;
  bool change_mode(Transfer_mode m, Baud_rate r) override;
  int tx_avail() const;
  void wait_tx_done() const {}
  inline void out_char(char c) const;
  int write(char const *s, unsigned long count,
            bool blocking = true) const override;

#ifndef UART_WITHOUT_INPUT
  int char_avail() const override;
  int get_char(bool blocking = true) const override;
#endif

private:
  mutable unsigned _current_rx_val = 0u;
  Io_register_block const *_rx_mbox = nullptr;
};

} // namespace L4
