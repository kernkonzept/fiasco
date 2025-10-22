/*
 * Copyright (C) 2021-2022 Stephan Gerhold <stephan@gerhold.net>
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/**
 * \file
 * Driver for Qualcomm "UART with Data Mover" (or sometimes serial_msm),
 * used in many older Qualcomm platforms such as the Snapdragon 210 and 410.
 *
 * At the moment the driver implements the minimum necessary to get serial
 * output/input and interrupts. Further optimizations would be possible,
 * e.g. by writing/reading 4 chars at a time when possible.
 */

#include "uart_dm.h"
#include "poll_timeout_counter.h"

namespace L4 {

enum
{
  DM_TFWR       = 0x01c,     // transmit FIFO watermark register
  DM_RFWR       = 0x020,     // receive FIFO watermark register
  DM_DMEN       = 0x03c,     // DMA / data packing
  DM_SR         = 0x0a4,     // status register
  DM_CR         = 0x0a8,     // command register
  DM_IMR        = 0x0b0,     // interrupt mask register
  DM_ISR        = 0x0b4,     // interrupt status register
  DM_TF         = 0x100,     // transmit FIFO register
  DM_RF         = 0x140,     // receive FIFO register

  DM_DMEN_TX_SC = 1 << 4,    // TX single character mode
  DM_DMEN_RX_SC = 1 << 5,    // RX single character mode

  DM_SR_RXRDY   = 1 << 0,    // receive FIFO has data
  DM_SR_RXFULL  = 1 << 1,    // receive FIFO is full
  DM_SR_TXRDY   = 1 << 2,    // transmit FIFO has space
  DM_SR_TXEMT   = 1 << 3,    // transmit FIFO empty

  DM_CR_RST_RX  = 0x01 << 4, // reset receiver
  DM_CR_RST_TX  = 0x02 << 4, // reset transmitter
  DM_CR_DIS_TX  = 1 << 3,    // disable transmitter
  DM_CR_ENA_TX  = 1 << 2,    // enable transmitter
  DM_CR_DIS_RX  = 1 << 1,    // disable receiver
  DM_CR_ENA_RX  = 1 << 0,    // enable receiver

  DM_IMR_RXLEV  = 1 << 4,    // RX FIFO above watermark level

  DM_RF_CHAR    = 0xff,      // (higher bits contain error information)
};

bool Uart_dm::startup(Io_register_block const *regs)
{
  _regs = regs;

  _regs->write32(DM_IMR, 0);
  _regs->write32(DM_CR, DM_CR_RST_RX);
  _regs->write32(DM_CR, DM_CR_RST_TX);
  // Use single character mode to read/write one char at a time
  _regs->write32(DM_DMEN, DM_DMEN_RX_SC | DM_DMEN_TX_SC);
  _regs->write32(DM_RFWR, 0); // Generate interrupt for every character
  _regs->write32(DM_CR, DM_CR_ENA_RX);
  _regs->write32(DM_CR, DM_CR_ENA_TX);

  return true;
}

void Uart_dm::shutdown()
{
  _regs->write32(DM_CR, DM_CR_DIS_RX);
  _regs->write32(DM_CR, DM_CR_DIS_TX);
}

bool Uart_dm::change_mode(Transfer_mode, Baud_rate)
{
  return true;
}

int Uart_dm::tx_avail() const
{
  return _regs->read32(DM_SR) & DM_SR_TXRDY;
}

void Uart_dm::out_char(char c) const
{
  _regs->write32(DM_TF, c);
}

void Uart_dm::wait_tx_done() const
{
  Poll_timeout_counter i(3000000);
  while (i.test(!(_regs->read32(DM_SR) & DM_SR_TXEMT)))
    ;
}

int Uart_dm::write(char const *s, unsigned long count, bool blocking) const
{
  return generic_write<Uart_dm>(s, count, blocking);
}

#ifndef UART_WITHOUT_INPUT

bool Uart_dm::enable_rx_irq(bool enable)
{
  if (enable)
    _regs->write32(DM_IMR, DM_IMR_RXLEV);
  else
    _regs->write32(DM_IMR, 0);
  return true;
}

int Uart_dm::char_avail() const
{
  return _regs->read32(DM_SR) & DM_SR_RXRDY;
}

int Uart_dm::get_char(bool blocking) const
{
  while (!char_avail())
    if (!blocking)
      return -1;

  return _regs->read32(DM_RF) & DM_RF_CHAR;
}

#endif // !UART_WITHOUT_INPUT

} // namespace L4
