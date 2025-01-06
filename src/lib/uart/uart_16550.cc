/*
 * Copyright (C) 2008-2009 Technische Universität Dresden.
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Alexander Warg <alexander.warg@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/*!
 * \file   uart_16550.cc
 * \brief  16550 UART implementation
 *
 * \date   2008-01-04
 *
 */
#include "uart_16550.h"
#include "poll_timeout_counter.h"

namespace L4
{
  bool Uart_16550::startup(Io_register_block const *regs)
  {
    _regs = regs;

    // Only do the detection on x86 where we want to avoid
    // to write to I/O ports unnecessarily.
    // On other platforms there are UARTs where this check
    // fails.
#if defined(__x86_64__) || defined(__i686__) || defined(__i386__)
    char scratch, scratch2, scratch3;

    scratch = _regs->read<unsigned char>(IER);
    _regs->write<unsigned char>(IER, 0);

    _regs->delay();

    scratch2 = _regs->read<unsigned char>(IER);
    _regs->write<unsigned char>(IER, 0xf);

    _regs->delay();

    scratch3 = _regs->read<unsigned char>(IER);
    _regs->write<unsigned char>(IER, scratch);

    if (!(scratch2 == 0x00 && scratch3 == 0x0f))
      return false;  // this is not the uart
#endif

    /* disable all rs-232 interrupts */
    _regs->write<unsigned char>(IER, _ier_bits);
    /* rts, and dtr enabled */
    _regs->write<unsigned char>(MCR, _mcr_bits | 3);
    /* enable fifo */
    _regs->write<unsigned char>(FCR, _fcr_bits | 1);
    /* clear line control register: set to (8N1) */
    _regs->write<unsigned char>(LCR, 3);

    /* clearall interrupts */
    _regs->read<unsigned char>(MSR); /* IRQID 0*/
    _regs->read<unsigned char>(IIR); /* IRQID 1*/
    _regs->read<unsigned char>(TRB); /* IRQID 2*/
    _regs->read<unsigned char>(LSR); /* IRQID 3*/

    Poll_timeout_counter i(5000000);
    while (i.test(_regs->read<unsigned char>(LSR) & LSR_DR))
      _regs->read<unsigned char>(TRB);

    return true;
  }

  void Uart_16550::shutdown()
  {
    _regs->write<unsigned char>(MCR, 6);
    _regs->write<unsigned char>(FCR, 0);
    _regs->write<unsigned char>(LCR, 0);
    _regs->write<unsigned char>(IER, 0);
  }

  bool Uart_16550::change_mode(Transfer_mode m, Baud_rate r)
  {
    unsigned long old_lcr = _regs->read<unsigned char>(LCR);
    if(r != BAUD_NC && _base_rate) {
      unsigned short divisor = _base_rate / r;
      _regs->write<unsigned char>(LCR, old_lcr | 0x80/*DLAB*/);
      _regs->write<unsigned char>(TRB, divisor & 0x0ff);        /* BRD_LOW  */
      _regs->write<unsigned char>(IER, (divisor >> 8) & 0x0ff); /* BRD_HIGH */
      _regs->write<unsigned char>(LCR, old_lcr);
    }
    if (m != MODE_NC)
      _regs->write<unsigned char>(LCR, m & 0x07f);

    return true;
  }

  int Uart_16550::get_char(bool blocking) const
  {
    char old_ier, ch;

    if (!blocking && !char_avail())
      return -1;

    old_ier = _regs->read<unsigned char>(IER);
    _regs->write<unsigned char>(IER, old_ier & ~0xf);
    while (!char_avail())
      ;
    ch = _regs->read<unsigned char>(TRB);
    _regs->write<unsigned char>(IER, old_ier);
    return ch;
  }

  int Uart_16550::char_avail() const
  {
    return _regs->read<unsigned char>(LSR) & LSR_DR;
  }

  int Uart_16550::tx_avail() const
  {
    return _regs->read<unsigned char>(LSR) & LSR_THRE;
  }

  void Uart_16550::wait_tx_done() const
  {
    Poll_timeout_counter i(5000000);
    while (i.test(!(_regs->read<unsigned char>(LSR) & LSR_TSRE)))
      ;
  }

  void Uart_16550::out_char(char c) const
  {
    _regs->write<unsigned char>(TRB, c);
  }

  int Uart_16550::write(char const *s, unsigned long count, bool blocking) const
  {
    /* disable UART IRQs */
    unsigned char old_ier = _regs->read<unsigned char>(IER);
    _regs->write<unsigned char>(IER, old_ier & ~0x0f);

    int c = generic_write<Uart_16550>(s, count, blocking);

    _regs->write<unsigned char>(IER, old_ier);
    return c;
  }

  bool Uart_16550::enable_rx_irq(bool enable)
  {
    unsigned char ier = _regs->read<unsigned char>(IER);
    bool res = ier & 1;
    if (enable)
      ier |= 1;
    else
      ier &= ~1;

    _regs->write<unsigned char>(IER, ier);
    return res;
  }
};
