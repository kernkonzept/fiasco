/*
 * Copyright (C) 2021-2024 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "uart_sifive.h"
#include "poll_timeout_counter.h"

namespace L4 {

enum : unsigned
{
  UARTSFV_TXDATA = 0x00, // Transmit data register
  UARTSFV_RXDATA = 0x04, // Receive data register
  UARTSFV_TXCTRL = 0x08, // Transmit control register
  UARTSFV_RXCTRL = 0x0C, // Receive control register
  UARTSFV_IE     = 0x10, // UART interrupt enable
  UARTSFV_IP     = 0x14, // UART interrupt pending
  UARTSFV_DIV    = 0x18, // Baud rate divisor

  UARTSFV_TXDATA_FULL  = 0x80000000,
  UARTSFV_RXDATA_EMPTY = 0x80000000,
  UARTSFV_RXDATA_DATA  = 0xff,

  UARTSFV_TXCTRL_TXEN = 1,
  UARTSFV_TXCTRL_TXCNT_SHIFT = 16,

  UARTSFV_RXCTRL_RXEN = 1,

  UARTSFV_IE_RXWM = 2,

  UARTSFV_IP_TXWM = 1,
  UARTSFV_IP_RXWM = 2,
};
bool Uart_sifive::startup(Io_register_block const *regs)
{
  _regs = regs;

  // Disable TX and RX interrupts
  _regs->write<unsigned>(UARTSFV_IE, 0);

  // Enable TX and set transmit watermark level to 1, i.e. it triggers when
  // the transmit FIFO is empty.
  _regs->write<unsigned>(UARTSFV_TXCTRL, UARTSFV_TXCTRL_TXEN
                                         | 1u << UARTSFV_TXCTRL_TXCNT_SHIFT);
  // Enable RX
  _regs->write<unsigned>(UARTSFV_RXCTRL, UARTSFV_RXCTRL_RXEN);

  return true;
}

void Uart_sifive::shutdown()
{
  _regs->write<unsigned>(UARTSFV_IE, 0);
  _regs->write<unsigned>(UARTSFV_TXCTRL, 0);
  _regs->write<unsigned>(UARTSFV_RXCTRL, 0);
}

bool Uart_sifive::change_mode(Transfer_mode, Baud_rate r)
{
  unsigned div = _freq / r;
  _regs->write<unsigned>(UARTSFV_DIV, div);

  return true;
}

int Uart_sifive::tx_avail() const
{
  return !(_regs->read<unsigned>(UARTSFV_TXDATA) & UARTSFV_TXDATA_FULL);
}

void Uart_sifive::wait_tx_done() const
{
  Poll_timeout_counter i(5000000);
  while (i.test(!(_regs->read<unsigned>(UARTSFV_IP) & UARTSFV_IP_TXWM)))
    ;
}

void Uart_sifive::out_char(char c) const
{
  _regs->write<unsigned>(UARTSFV_TXDATA, c);
}

int Uart_sifive::write(char const *s, unsigned long count, bool blocking) const
{
  return generic_write<Uart_sifive>(s, count, blocking);
}

bool Uart_sifive::enable_rx_irq(bool enable)
{
  if (enable)
    _regs->set<unsigned>(UARTSFV_IE, UARTSFV_IE_RXWM);
  else
    _regs->clear<unsigned>(UARTSFV_IE, UARTSFV_IE_RXWM);
  return true;
}

int Uart_sifive::char_avail() const
{
  if (_bufchar == -1)
    {
      unsigned data = _regs->read<unsigned>(UARTSFV_RXDATA);
      if (!(data & UARTSFV_RXDATA_EMPTY))
        _bufchar = data & UARTSFV_RXDATA_DATA;
    }
  return _bufchar != -1;
}

int Uart_sifive::get_char(bool blocking) const
{
  while (!char_avail())
    if (!blocking)
      return -1;

  int c = _bufchar;
  _bufchar = -1;
  return c;
}

} // namespace L4
