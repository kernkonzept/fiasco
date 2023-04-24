/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 */
/*
 * (c) 2015
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/*!
 * \file   uart_16550_dw.cc
 * \brief  Implementation of DW-based 16550 UART
 *
 * \author Adam Lackorzynski <adam@l4re.org>
 *
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
