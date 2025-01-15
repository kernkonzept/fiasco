/*
 * Copyright (C) 2016, 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "uart_sh.h"
#include "poll_timeout_counter.h"

namespace L4
{
  enum
  {
    SCSMR  = 0x00,
    SCBRR  = 0x04,
    SCSCR  = 0x08,
    SCFTDR = 0x0C,
    SCFSR  = 0x10,
    SCFRDR = 0x14,
    SCFCR  = 0x18,
    SCFDR  = 0x1C,
    SCSPTR = 0x20,
    SCLSR  = 0x24,
    DL     = 0x28,
    CKS    = 0x2C,
  };

  enum
  {
    SR_DR    = 1 << 0, // Data ready
    SR_RDF   = 1 << 1, // Receive FIFO full
    SR_BRK   = 1 << 4, // Break Detect
    SR_TDFE  = 1 << 5, // Transmit FIFO data empty
    SR_TEND  = 1 << 6, // Transmission end
    SR_ER    = 1 << 7, // Receive Error

    SCR_RE   = 1 << 4, // Receive enable
    SCR_TE   = 1 << 5, // Transmit enable
    SCR_RIE  = 1 << 6, // Receive IRQ enable
    SCR_TIE  = 1 << 7, // Transmit IRQ enable

    LSR_ORER = 1 << 0, // Overrun error
  };

  bool Uart_sh::startup(Io_register_block const *regs)
  {
    _regs = regs;
    _regs->write<unsigned short>(SCSCR, SCR_RE | SCR_TE);
    return true;
  }

  bool Uart_sh::enable_rx_irq(bool enable)
  {
    if (enable)
      _regs->set<unsigned short>(SCSCR, SCR_RIE);
    else
      _regs->clear<unsigned short>(SCSCR, SCR_RIE);
    return true;
  }

  void Uart_sh::shutdown()
  {
    _regs->write<unsigned short>(SCSCR, 0);
  }

  bool Uart_sh::change_mode(Transfer_mode, Baud_rate r)
  {
    static_cast<void>(r);
    // Set 8N1 and clock rate in SCSMR
    return true;
  }

  void Uart_sh::irq_ack()
  {
    unsigned short status = _regs->read<unsigned short>(SCLSR);
    if (status & LSR_ORER) // Overrun error
      _regs->clear<unsigned short>(SCLSR, LSR_ORER);
  }

  int Uart_sh::get_char(bool blocking) const
  {
    int sr;
    while (!(sr = char_avail()))
      if (!blocking)
        return -1;

    int ch;
    if (sr & SR_BRK)
      ch = -1;
    else
      ch = _regs->read<unsigned char>(SCFRDR);
    _regs->clear<unsigned short>(SCFSR, SR_DR | SR_RDF | SR_ER | SR_BRK);
    return ch;
  }

  int Uart_sh::char_avail() const
  {
    return _regs->read<unsigned short>(SCFSR) & (SR_DR | SR_RDF | SR_BRK);
  }

  int Uart_sh::tx_avail() const
  {
    return _regs->read<unsigned short>(SCFSR) & SR_TDFE;
  }

  void Uart_sh::out_char(char c) const
  {
    _regs->write<unsigned char>(SCFTDR, c);
    _regs->clear<unsigned short>(SCFSR, SR_TEND | SR_TDFE);
  }

  int Uart_sh::write(char const *s, unsigned long count, bool blocking) const
  {
    return generic_write<Uart_sh>(s, count, blocking);
  }
}
