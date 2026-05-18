/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "uart_stm32.h"
#include "poll_timeout_counter.h"

namespace L4 {

enum
{
  CR1   = 0x00,
  CR2   = 0x04,
  CR3   = 0x08,
  BRR   = 0x0c,
  GTPR  = 0x10,
  RTOR  = 0x14,
  RQR   = 0x18,
  ISR   = 0x1c,
  ICR   = 0x20,
  RDR   = 0x24,
  TDR   = 0x28,

  CR1_UE     = 1 <<  0, // USART enable
  CR1_RE     = 1 <<  2, // Receiver enable
  CR1_TE     = 1 <<  3, // Transmitter enable
  CR1_RXNEIE = 1 <<  5, // RXNE / RXFNE interrupt enable
  CR1_OVER8  = 1 << 15, // Oversampling mode (0: 16x, 1: 8x)

  ISR_RXNE   = 1 <<  5, // Read data register not empty / RX FIFO not empty
  ISR_TC     = 1 <<  6, // Transmission complete
  ISR_TXE    = 1 <<  7, // TX data register empty / TX FIFO not full
};

bool Uart_stm32::startup(Io_register_block const *regs)
{
  _regs = regs;
  // Disable first to allow configuration of other registers.
  _regs->clear<unsigned>(CR1, CR1_UE);
  _regs->write<unsigned>(CR2, 0);
  _regs->write<unsigned>(CR3, 0);
  // 8N1, 16x oversampling, TX + RX enabled, then enable the USART.
  _regs->write<unsigned>(CR1, CR1_TE | CR1_RE);
  _regs->set<unsigned>(CR1, CR1_UE);
  return true;
}

void Uart_stm32::shutdown()
{
  _regs->clear<unsigned>(CR1, CR1_UE | CR1_TE | CR1_RE | CR1_RXNEIE);
}

bool Uart_stm32::change_mode(Transfer_mode, Baud_rate r)
{
  if (_freq && r)
    {
      // 16x oversampling: BRR = fck / baud
      _regs->clear<unsigned>(CR1, CR1_UE);
      _regs->clear<unsigned>(CR1, CR1_OVER8);
      _regs->write<unsigned>(BRR, _freq / r);
      _regs->set<unsigned>(CR1, CR1_UE);
    }
  return true;
}

bool Uart_stm32::tx_avail() const
{
  return _regs->read<unsigned>(ISR) & ISR_TXE;
}

void Uart_stm32::wait_tx_done() const
{
  Poll_timeout_counter i(3000000);
  while (i.test(!(_regs->read<unsigned>(ISR) & ISR_TC)))
    ;
}

void Uart_stm32::out_char(char c) const
{
  _regs->write<unsigned>(TDR, static_cast<unsigned char>(c));
}

int Uart_stm32::write(char const *s, unsigned long count, bool blocking) const
{
  return generic_write<Uart_stm32>(s, count, blocking);
}

#ifndef UART_WITHOUT_INPUT

bool Uart_stm32::enable_rx_irq(bool enable)
{
  if (enable)
    _regs->set<unsigned>(CR1, CR1_RXNEIE);
  else
    _regs->clear<unsigned>(CR1, CR1_RXNEIE);
  return true;
}

int Uart_stm32::char_avail() const
{
  return (_regs->read<unsigned>(ISR) & ISR_RXNE) != 0;
}

int Uart_stm32::get_char(bool blocking) const
{
  while (!char_avail())
    if (!blocking)
      return -1;

  return _regs->read<unsigned>(RDR) & 0xff;
}

#endif // !UART_WITHOUT_INPUT

} // namespace L4

static l4re_device_spec_dt_ids dt_ids[] = {
  { .compatible = "st,stm32f7-uart" },
  { .compatible = "st,stm32h7-uart" },
  {},
};

l4re_register_device_uart_dt(L4::Uart_stm32, stm32, dt_ids);
