/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 */
/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
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
  class Uart_imx : public Uart
  {
  public:
    enum platform_type
    {
      Type_imx21,
      Type_imx35,
      Type_imx51,
      Type_imx6,
      Type_imx7,
      Type_imx8,
    };
    explicit Uart_imx(enum platform_type type) : _type(type) {}
    explicit Uart_imx(enum platform_type type, unsigned /*base_rate*/)
      : _type(type) {}
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool enable_rx_irq(bool enable = true) override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    int tx_avail() const;
    void wait_tx_done() const;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count,
              bool blocking = true) const override;

  private:
    enum platform_type _type;
  };

  class Uart_imx21 : public Uart_imx
  {
  public:
    Uart_imx21() : Uart_imx(Type_imx21) {}
    explicit Uart_imx21(unsigned base_rate) : Uart_imx(Type_imx21, base_rate) {}
  };

  class Uart_imx35 : public Uart_imx
  {
  public:
    Uart_imx35() : Uart_imx(Type_imx35) {}
    explicit Uart_imx35(unsigned base_rate) : Uart_imx(Type_imx35, base_rate) {}
  };

  class Uart_imx51 : public Uart_imx
  {
  public:
    Uart_imx51() : Uart_imx(Type_imx51) {}
    explicit Uart_imx51(unsigned base_rate) : Uart_imx(Type_imx51, base_rate) {}
  };

  class Uart_imx6 : public Uart_imx
  {
  public:
    Uart_imx6() : Uart_imx(Type_imx6) {}
    explicit Uart_imx6(unsigned base_rate) : Uart_imx(Type_imx6, base_rate) {}

    void irq_ack() override;
  };

  class Uart_imx7 : public Uart_imx
  {
  public:
    Uart_imx7() : Uart_imx(Type_imx7) {}
    explicit Uart_imx7(unsigned base_rate) : Uart_imx(Type_imx7, base_rate) {}
  };

  class Uart_imx8 : public Uart_imx
  {
  public:
    Uart_imx8() : Uart_imx(Type_imx8) {}
    explicit Uart_imx8(unsigned base_rate) : Uart_imx(Type_imx8, base_rate) {}
  };
};
