/*
 * Copyright (C) 2018 Technische Universität Dresden.
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "uart_base.h"

namespace L4 {

class Uart_linflex : public Uart
{
public:
  explicit Uart_linflex(unsigned) {}
  bool startup(Io_register_block const *) override;
  void shutdown() override;
  bool change_mode(Transfer_mode m, Baud_rate r) override;
  int tx_avail() const;
  void wait_tx_done() const;
  inline void out_char(char c) const;
  int write(char const *s, unsigned long count,
            bool blocking = true) const override;

#ifndef UART_WITHOUT_INPUT
  bool enable_rx_irq(bool enable = true) override;
  int char_avail() const override;
  int get_char(bool blocking = true) const override;
#endif

private:
  void set_uartcr(bool fifo);
  bool is_tx_fifo_enabled() const;
  bool is_rx_fifo_enabled() const;
};

} // namespace L4
