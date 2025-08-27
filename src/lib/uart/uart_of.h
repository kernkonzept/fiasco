/*
 * Copyright (C) 2009 Technische Universität Dresden.
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "uart_base.h"
#include <stdarg.h>
#include <string.h>
#include <l4/drivers/of.h>

namespace L4 {

class Uart_of : public Uart, public L4_drivers::Of
{
private:
  ihandle_t  _serial;

public:
  Uart_of() : Of(), _serial(0) {}
  explicit Uart_of(unsigned /*base_rate*/) : Of(), _serial(0) {}
  bool startup(Io_register_block const *) override;
  void shutdown() override;
  bool change_mode(Transfer_mode m, Baud_rate r) override;
  int tx_avail() const;
  void out_char(char c) const;
  int write(char const *s, unsigned long count,
            bool blocking = true) const override;

#ifndef UART_WITHOUT_INPUT
  int char_avail() const override;
  int get_char(bool blocking = true) const override;
#endif
};

} // namespace L4
