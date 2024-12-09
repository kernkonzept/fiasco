/*
 * Copyright (C) 2015 Technische Universität Dresden.
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/*!
 * \file   uart_16550_dw.cc
 * \brief  Implementation of DW-based 16550 UART
 */
#include "uart_16550_dw.h"

namespace L4
{

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
      _regs->write<U8>(lcr, LCR);
    }
}

}
