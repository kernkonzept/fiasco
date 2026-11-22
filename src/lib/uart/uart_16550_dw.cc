/*
 * Copyright (C) 2015 Technische Universität Dresden.
 * Copyright (C) 2023-2025 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/*!
 * \file   uart_16550_dw.cc
 * \brief  Implementation of DW-based 16550 UART
 */
#include "uart_16550_dw.h"

namespace L4 {

#ifndef UART_WITHOUT_INPUT

void Uart_16550_dw::irq_ack()
{
  enum Registers_dw
  {
    DW_USR = 0x1f,
  };
  typedef unsigned char U8;
  U8 iir = _regs->read<U8>(IIR);

  if ((iir & IIR_BUSY) == IIR_BUSY)
    {
      U8 lcr = _regs->read<U8>(LCR);
      U8 usr = _regs->read<U8>(DW_USR);
      asm volatile("" : : "r" (usr) : "memory");
      _regs->write<U8>(LCR, lcr);
    }
}

#endif // !UART_WITHOUT_INPUT

} // namespace L4

static const l4re_device_spec_dt_ids dt_ids[] = {
  { .compatible = "snps,dw-apb-uart" },
  {},
};

l4re_register_device_uart_dt(L4::Uart_16550_dw, uart16550dw, dt_ids);
