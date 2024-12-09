/*
 * Copyright (C) 2011 Technische Universität Dresden.
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Björn Döbel <doebel@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "uart_leon3.h"
#include "poll_timeout_counter.h"

namespace L4
{
  enum {
    DATA_REG   = 0x00,
    STATUS_REG = 0x04,
    CTRL_REG   = 0x08,
    SCALER_REG = 0x0C,

    DATA_MASK  = 0x0F,

    STATUS_DR  = 0x001, // data ready
    STATUS_TS  = 0x002, // transmit shift empty
    STATUS_TE  = 0x004, // transmit fifo empty
    STATUS_BR  = 0x008, // BREAK received
    STATUS_OV  = 0x010, // overrun
    STATUS_PE  = 0x020, // parity error
    STATUS_FE  = 0x040, // framing error
    STATUS_TH  = 0x080, // transmit fifo half-full
    STATUS_RH  = 0x100, // recv fifo half-full
    STATUS_TF  = 0x200, // transmit fifo full
    STATUS_RF  = 0x400, // recv fifo full

    STATUS_TCNT_MASK  = 0x3F,
    STATUS_TCNT_SHIFT = 20,
    STATUS_RCNT_MASK  = 0x3F,
    STATUS_RCNT_SHIFT = 26,

    CTRL_RE    = 0x001,
    CTRL_TE    = 0x002,
    CTRL_RI    = 0x004,
    CTRL_TI    = 0x008,
    CTRL_PS    = 0x010,
    CTRL_PE    = 0x020,
    CTRL_FL    = 0x040,
    CTRL_LB    = 0x080,
    CTRL_EC    = 0x100,
    CTRL_TF    = 0x200,
    CTRL_RF    = 0x400,

    SCALER_MASK = 0xFFF,
  };


  bool Uart_leon3::startup(Io_register_block const *regs)
  {
    _regs = regs;
    _regs->write<unsigned int>(CTRL_REG, CTRL_TE | CTRL_RE);

    return true;
  }

  void Uart_leon3::shutdown()
  { }

  bool Uart_leon3::change_mode(Transfer_mode, Baud_rate r)
  {
    if (r != 115200)
      return false;

    return true;
  }

  bool Uart_leon3::enable_rx_irq(bool enable)
  {
    unsigned r = _regs->read<unsigned int>(CTRL_REG);

    if (enable)
      r |= CTRL_RI;
    else
      r &= ~CTRL_RI;

    _regs->write<unsigned int>(CTRL_REG, r);
    return 0;
  }

  int Uart_leon3::get_char(bool blocking) const
  {
    while (!char_avail())
      if (!blocking)
        return -1;

    return _regs->read<unsigned int>(DATA_REG) & DATA_MASK;
  }

  int Uart_leon3::char_avail() const
  {
    return _regs->read<unsigned int>(STATUS_REG) & STATUS_DR;
  }

  int Uart_leon3::tx_avail() const
  {
    return !(_regs->read<unsigned int>(STATUS_REG) & STATUS_TF);
  }

  void Uart_leon3::wait_tx_done() const
  {
    enum { Tx_empty = STATUS_TS | STATUS_TE }; /* be conservative */
    Poll_timeout_counter i(3000000);
    while (i.test((_regs->read<unsigned int>(STATUS_REG) & Tx_empty) != Tx_empty))
      ;
  }

  void Uart_leon3::out_char(char c) const
  {
    _regs->write<unsigned int>(DATA_REG, c);
  }

  int Uart_leon3::write(char const *s, unsigned long count, bool blocking) const
  {
    return generic_write<Uart_leon3>(s, count, blocking);
  }
};
