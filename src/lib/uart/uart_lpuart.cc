/*
 * Copyright (C) 2019, 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "uart_lpuart.h"
#include "poll_timeout_counter.h"

namespace L4 {

enum
{
  VERID  = 0x00,
  PARAM  = 0x04,
  GLOBAL = 0x08,
  PINCFG = 0x0c,
  BAUD   = 0x10,
  STAT   = 0x14,
  CTRL   = 0x18,
  DATA   = 0x1c,
  MATCH  = 0x20,
  MODIR  = 0x24,
  FIFO   = 0x28,
  WATER  = 0x2c,

  CTRL_RE       = 1 << 18,
  CTRL_TE       = 1 << 19,
  CTRL_RIE      = 1 << 21,

  FIFO_RXEMPT   = 1 << 22, // Receive buffer is empty?
  FIFO_TXEMPT   = 1 << 23, // Transmit FIFO empty?

  BAUD_BOTHEDGE = 1 << 17,
};

bool Uart_lpuart::startup(Io_register_block const *regs)
{
  _regs = regs;
  _regs->write<unsigned>(CTRL, CTRL_RE | CTRL_TE);
  return true;
}

void Uart_lpuart::shutdown()
{
  _regs->write<unsigned>(CTRL, 0);
}

bool Uart_lpuart::change_mode(Transfer_mode, Baud_rate r)
{
  if (_freq)
    {
      // The following needs to use _freq to derive values from
      enum
      {
        BAUD_CLOCK    = 80064000,
        BAUD_OSR_VAL  = 4,
        BAUD_OSR      = BAUD_OSR_VAL << 24,
      };

      _regs->clear<unsigned>(CTRL, CTRL_RE | CTRL_TE);

      _regs->write<unsigned>(BAUD,
                               ((BAUD_CLOCK / r / (BAUD_OSR_VAL + 1)) & 0x1fff)
                             | BAUD_OSR);

      _regs->set<unsigned>(CTRL, CTRL_RE | CTRL_TE);
    }
  return true;
}

int Uart_lpuart::tx_avail() const
{
  return _regs->read<unsigned>(FIFO) & FIFO_TXEMPT;
}

void Uart_lpuart::out_char(char c) const
{
  _regs->write<unsigned char>(DATA, c);
}

int Uart_lpuart::write(char const *s, unsigned long count, bool blocking) const
{
  return generic_write<Uart_lpuart>(s, count, blocking);
}

#ifndef UART_WITHOUT_INPUT

bool Uart_lpuart::enable_rx_irq(bool enable)
{
  if (enable)
    _regs->set<unsigned>(CTRL, CTRL_RIE);
  else
    _regs->clear<unsigned>(CTRL, CTRL_RIE);

  return true;
}

int Uart_lpuart::char_avail() const
{
  return !(_regs->read<unsigned>(FIFO) & FIFO_RXEMPT);
}

int Uart_lpuart::get_char(bool blocking) const
{
  while (!char_avail())
    if (!blocking)
      return -1;

  return _regs->read<unsigned char>(DATA);
}

#endif // !UART_WITHOUT_INPUT

} // namespace L4
