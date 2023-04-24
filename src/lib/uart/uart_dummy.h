/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 */
/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "uart_base.h"

namespace L4
{
  class Uart_dummy : public Uart
  {
  public:
    explicit Uart_dummy() {}
    explicit Uart_dummy(unsigned /*base_rate*/) {}
    bool startup(Io_register_block const *) override { return true; }
    void shutdown() override {}
    bool change_mode(Transfer_mode, Baud_rate) override { return true; }
    int get_char(bool /*blocking*/ = true) const override { return 0; }
    int char_avail() const override { return false; }
    inline void out_char(char /*ch*/) const {}
    int write(char const * /*str*/, unsigned long /*count*/,
              bool /*blocking*/ = true) const override
    { return 0; }
  };
};
