/*
 * Copyright (C) 2008-2012 Technische Universität Dresden.
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Alexander Warg <alexander.warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "uart_base.h"

namespace L4 {

class Uart_sa1000 : public Uart
{
public:
  explicit Uart_sa1000() {}
  explicit Uart_sa1000(unsigned /*base_rate*/) {}
  bool startup(Io_register_block const *) override;
  void shutdown() override;
  bool change_mode(Transfer_mode m, Baud_rate r) override;
  int tx_avail() const;
  void wait_tx_done() const;
  inline void out_char(char c) const;
  int write(char const *s, unsigned long count,
            bool blocking = true) const override;

#ifndef UART_WITHOUT_INPUT
  int char_avail() const override;
  int get_char(bool blocking = true) const override;
#endif
};

} // namespace L4
