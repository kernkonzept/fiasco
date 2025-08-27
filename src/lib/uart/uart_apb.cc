/*
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "uart_apb.h"
#include "poll_timeout_counter.h"

namespace L4 {

enum Registers
{
  DATA      = 0x00,
  STATE     = 0x04,
  CTRL      = 0x08,
  INT       = 0x0c,
  BAUDDIV   = 0x10,

  STATE_TX_FULL   = 1 << 0, // TX buffer full, read-only.
  STATE_RX_FULL   = 1 << 1, // RX buffer full, read-only.
  STATE_TX_OVERUN = 1 << 2, // TX buffer overrun, write 1 to clear.
  STATE_RX_OVERUN = 1 << 3, // RX buffer overrun, write 1 to clear.

  CTRL_TX_EN            = 1 << 0, // TX enable.
  CTRL_RX_EN            = 1 << 1, // RX enable.
  CTRL_TX_INT_EN        = 1 << 2, // TX interrupt enable.
  CTRL_RX_INT_EN        = 1 << 3, // RX interrupt enable.
  CTRL_TX_OVERUN_INT_EN = 1 << 4, // TX overrun interrupt enable.
  CTRL_RX_OVERUN_INT_EN = 1 << 5, // RX overrun interrupt enable.
  CTRL_TX_HIGH_SPEED    = 1 << 6, // High-speed test mode for TX only.

  INT_TX         = 1 << 0, // TX interrupt. Write 1 to clear.
  INT_RX         = 1 << 1, // RX interrupt. Write 1 to clear.
  INT_TX_OVERRUN = 1 << 2, // TX overrun interrupt. Write 1 to clear.
  INT_RX_OVERRUN = 1 << 3, // RX overrun interrupt. Write 1 to clear.

  // TODO: How to handle chars lost due to overrun?

  Default_baud = 115200,
};

void Uart_apb::set_rate(Baud_rate r)
{
  _regs->write<unsigned int>(BAUDDIV, _freq / r);
}

bool Uart_apb::startup(Io_register_block const *regs)
{
  _regs = regs;
  if (_freq)
    set_rate(Default_baud);

  _regs->write<unsigned char>(CTRL, CTRL_TX_EN | CTRL_RX_EN | CTRL_TX_HIGH_SPEED);
  wait_tx_done();
  return true;
}

void Uart_apb::shutdown()
{
  _regs->write<unsigned char>(CTRL, 0);
  _regs->write<unsigned char>(STATE, STATE_TX_OVERUN | STATE_RX_OVERUN);
  _regs->write<unsigned char>(INT, INT_TX | INT_RX | INT_TX_OVERRUN | INT_RX_OVERRUN);
}

bool Uart_apb::change_mode(Transfer_mode, Baud_rate r)
{
  if (_freq)
    set_rate(r);

  return true;
}

int Uart_apb::tx_avail() const
{
  return !(_regs->read<unsigned char>(STATE) & STATE_TX_FULL);
}

void Uart_apb::wait_tx_done() const
{
  Poll_timeout_counter i(3000000);
  while (i.test(!tx_avail()))
     ;
}

void Uart_apb::out_char(char c) const
{
  _regs->write<unsigned char>(DATA, c);
}

int Uart_apb::write(char const *s, unsigned long count, bool blocking) const
{
  return generic_write<Uart_apb>(s, count, blocking);
}

bool Uart_apb::enable_rx_irq(bool enable)
{
  if (enable)
    _regs->set<unsigned char>(CTRL, CTRL_RX_INT_EN);
  else
    _regs->clear<unsigned char>(CTRL, CTRL_RX_INT_EN);
  return true;
}

#ifndef UART_WITHOUT_INPUT

int Uart_apb::char_avail() const
{
  return _regs->read<unsigned char>(STATE) & STATE_RX_FULL;
}

int Uart_apb::get_char(bool blocking) const
{
  while (!char_avail())
    if (!blocking)
      return -1;

  return _regs->read<unsigned char>(DATA);
}

#endif // !UART_WITHOUT_INPUT

} // namespace L4
