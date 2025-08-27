/*
 * Copyright (C) 2017, 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "uart_mvebu.h"
#include "poll_timeout_counter.h"

namespace L4 {

enum
{
  // use names from manual
  RBR  = 0x00,
  TSH  = 0x04,
  CTRL = 0x08,
  STAT = 0x0c,
  BRDV = 0x10,

  STAT_RX_RDY       = 0x0010,
  STAT_TXFIFO_FULL  = 0x0800,

  CTRL_RX_RDY_INT_EN = 1 <<  4,
  CTRL_RXFIFO_RST    = 1 << 14,
  CTRL_TXFIFO_RST    = 1 << 15,
};

bool Uart_mvebu::startup(Io_register_block const *regs)
{
  _regs = regs;

  regs->write<unsigned>(CTRL, CTRL_RXFIFO_RST | CTRL_TXFIFO_RST);
  regs->write<unsigned>(CTRL, 0);

  return true;
}

void Uart_mvebu::shutdown()
{
  //_regs->write<unsigned>(CTRL, 0);
}

bool Uart_mvebu::change_mode(Transfer_mode, Baud_rate r)
{
  _regs->write<unsigned>(BRDV, _baserate / r / 16);
  return true;
}

int Uart_mvebu::tx_avail() const
{
  return !(_regs->read<unsigned>(STAT) & STAT_TXFIFO_FULL);
}

void Uart_mvebu::out_char(char c) const
{
  _regs->write<unsigned>(TSH, c);
  //_regs->clear<unsigned short>(SCFSR, SR_TEND | SR_TDFE);
}

int Uart_mvebu::write(char const *s, unsigned long count, bool blocking) const
{
  return generic_write<Uart_mvebu>(s, count, blocking);
}

#ifndef UART_WITHOUT_INPUT

bool Uart_mvebu::enable_rx_irq(bool enable)
{
  if (enable)
    _regs->set<unsigned>(CTRL, CTRL_RX_RDY_INT_EN);
  else
    _regs->clear<unsigned>(CTRL, CTRL_RX_RDY_INT_EN);
  return true;
}

int Uart_mvebu::char_avail() const
{
  return _regs->read<unsigned>(STAT) & STAT_RX_RDY;
}

int Uart_mvebu::get_char(bool blocking) const
{
  while (!char_avail())
    if (!blocking)
      return -1;

  return _regs->read<unsigned>(RBR) & 0xff;
}

#endif // !UART_WITHOUT_INPUT

} // namespace L4
