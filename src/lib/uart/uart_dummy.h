/*
 * Copyright 2009 Technische Universität Dresden.
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "uart_base.h"

namespace L4 {

class Uart_dummy : public Uart
{
public:
  explicit Uart_dummy() {}
  explicit Uart_dummy(unsigned /*base_rate*/) {}
  bool startup(Io_register_block const *) override { return true; }
  void shutdown() override {}
  bool change_mode(Transfer_mode, Baud_rate) override { return true; }
  inline void out_char(char /*ch*/) const {}
  int write(char const * /*str*/, unsigned long /*count*/,
            bool /*blocking*/ = true) const override
  { return 0; }

  int char_avail() const override { return false; }
  int get_char(bool /*blocking*/ = true) const override { return 0; }
};

} // namespace L4
