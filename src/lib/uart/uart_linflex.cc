/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 */
/*
 * (c) 2018 Adam Lackorzynski <adam@l4re.org>
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "uart_linflex.h"
#include "poll_timeout_counter.h"

namespace L4
{

  enum
  {
    LINCR1 = 0x00,
    LINIER = 0x04,
    LINSR  = 0x08,
    UARTCR = 0x10,
    UARTSR = 0x14,
    BDRL   = 0x38,
    BDRM   = 0x3c,

    LINCR1_INIT = 1 << 0,
    LINCR1_MME  = 1 << 4,
    LINCR1_BF   = 1 << 7,

    LINSR_LINS  = 0xf << 12,
    LINSR_LINS_INITMODE = 1 << 12,

    LINIER_DRIE = 1 << 2,

    Fifo_mode = 0, // Fifo_mode == 0 -> Buffer-mode

    UARTCR_UART     = 1 << 0,
    UARTCR_WL0      = 1 << 1,
    UARTCR_PC0      = 1 << 3,
    UARTCR_TXEN     = 1 << 4,
    UARTCR_RXEN     = 1 << 5,
    UARTCR_PC1      = 1 << 6,
    UARTCR_WL1      = 0 << 7,
    UARTCR_TFBM     = 1 << 8,
    UARTCR_RFBM     = 1 << 9,
    UARTCR_RDFL_RFC_buffer = 0 << 10,
    UARTCR_TDFL_TFC_buffer = 0 << 13,
    UARTCR_RDFL_RFC_fifo   = 4 << 10,
    UARTCR_TDFL_TFC_fifo   = 4 << 13,
    UARTCR_NEF      = 0 << 20,

    UARTSR_DTFTFF = 1 << 1,
    UARTSR_DRFRFE = 1 << 2,
    UARTSR_RMB    = 1 << 9,
  };

  bool Uart_linflex::startup(Io_register_block const *regs)
  {
    _regs = regs;

    _regs->write<unsigned>(LINCR1, LINCR1_INIT | LINCR1_MME | LINCR1_BF);
    while ((_regs->read<unsigned>(LINSR) & LINSR_LINS) != LINSR_LINS_INITMODE)
      ;

    _regs->write<unsigned>(UARTCR, UARTCR_UART);

    /* Do not use FIFO mode for RX, IRQs do not work with FIFO mode
     * (or something is wrong in the config whatsoever...) */
    unsigned v = UARTCR_UART
                 | UARTCR_WL0
                 | UARTCR_PC0
                 | UARTCR_PC1
                 | UARTCR_TXEN
                 | UARTCR_RXEN
                 | UARTCR_NEF
                 ;

    if (Fifo_mode)
      v |=   UARTCR_RDFL_RFC_fifo | UARTCR_TDFL_TFC_fifo
           | UARTCR_TFBM | UARTCR_RFBM;
    else
      v |= UARTCR_RDFL_RFC_buffer | UARTCR_TDFL_TFC_buffer;

    _regs->write<unsigned>(UARTCR, v);
    _regs->write<unsigned int>(UARTSR, UARTSR_DRFRFE | UARTSR_RMB);
    _regs->clear<unsigned>(LINCR1, LINCR1_INIT);

    return true;
  }

  bool Uart_linflex::enable_rx_irq(bool enable)
  {
    _regs->set<unsigned>(LINCR1, LINCR1_INIT);
    while ((_regs->read<unsigned>(LINSR) & LINSR_LINS) != LINSR_LINS_INITMODE)
      ;
    if (enable)
      _regs->set<unsigned>(LINIER, LINIER_DRIE);
    else
      _regs->clear<unsigned>(LINIER, LINIER_DRIE);
    _regs->clear<unsigned>(LINCR1, LINCR1_INIT);

    return true;
  }

  void Uart_linflex::shutdown()
  {
  }

  bool Uart_linflex::change_mode(Transfer_mode, Baud_rate r)
  {
    (void)r;
    return true;
  }

  int Uart_linflex::get_char(bool blocking) const
  {
    while (!char_avail())
      if (!blocking)
        return -1;

    unsigned c;
    if (Fifo_mode)
      {
        unsigned sr = _regs->read<unsigned>(UARTSR);
        _regs->write<unsigned int>(UARTSR, sr);
        c = _regs->read<unsigned char>(BDRM);
      }
    else
      {
        unsigned sr = _regs->read<unsigned>(UARTSR);
        c = _regs->read<unsigned char>(BDRM);
        _regs->write<unsigned int>(UARTSR, sr | UARTSR_DRFRFE | UARTSR_RMB);
      }
    return c;
  }

  int Uart_linflex::char_avail() const
  {
    if (Fifo_mode)
      return !(_regs->read<unsigned>(UARTSR) & UARTSR_DRFRFE);
    return _regs->read<unsigned>(UARTSR) & UARTSR_RMB;
  }

  int Uart_linflex::tx_avail() const
  {
    if (!Fifo_mode)
      return true;

    return !(_regs->read<unsigned>(UARTSR) & UARTSR_DTFTFF);
  }

  void Uart_linflex::wait_tx_done() const
  {
    // Already done in out_char() after writing a single character.
  }

  void Uart_linflex::out_char(char c) const
  {
    _regs->write<unsigned char>(BDRL, c);

    Poll_timeout_counter i(3000000);
    while (i.test(!(_regs->read<unsigned>(UARTSR) & UARTSR_DTFTFF)))
      ;
    _regs->write<unsigned>(UARTSR, UARTSR_DTFTFF);
  }

  int Uart_linflex::write(char const *s, unsigned long count, bool blocking) const
  {
    return generic_write<Uart_linflex>(s, count, blocking);
  }
}
