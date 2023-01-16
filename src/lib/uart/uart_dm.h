/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2022 Stephan Gerhold <stephan@gerhold.net>
 * Copyright (C) 2022-2023 Kernkonzept GmbH.
 */
#pragma once

#include "uart_base.h"

namespace L4
{
  class Uart_dm : public Uart
  {
  public:
    explicit Uart_dm(unsigned /*base_rate*/) {}
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    bool enable_rx_irq(bool enable = true) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    void out_char(char c) const;
    int write(char const *s, unsigned long count) const override;
  };
};
