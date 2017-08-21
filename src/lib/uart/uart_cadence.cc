/*
 * (c) 2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "uart_cadence.h"
#include "poll_timeout_counter.h"

namespace L4
{
  enum
  {
    CR                       = 0x00,
    MR                       = 0x04,
    IER                      = 0x08,
    IDR                      = 0x0C,
    IMR                      = 0x10,
    ISR                      = 0x14,
    BAUDGEN                  = 0x18,
    RXTOUT                   = 0x1C,
    RXWM                     = 0x20,
    MODEMCR                  = 0x24,
    MODEMSR                  = 0x28,
    SR                       = 0x2C,
    FIFO                     = 0x30,
    Baud_rate_divider_reg0   = 0x34,
    Flow_delay_reg0          = 0x38,
    Tx_FIFO_trigger_level0   = 0x44,
  };

  namespace Ctrl
  {
    enum {
      Rxres = 1 << 0,
      Txres = 1 << 1,
      Rxen  = 1 << 2,
      Rxdis = 1 << 3,
      Txen  = 1 << 4,
      Txdis = 1 << 5,
      Rstto = 1 << 6,
    };
  };

  enum
  {
    IXR_TXFULL  = 1 << 4,
    IXR_TXEMPTY = 1 << 3,
    IXR_RXFULL  = 1 << 2,
    IXR_RXEMPTY = 1 << 1,
    IXR_RXOVR   = 1 << 0,
  };

  bool Uart_cadence::startup(Io_register_block const *regs)
  {
    _regs = regs;
    _regs->write<unsigned>(CR, Ctrl::Txres | Ctrl::Rxres);
    change_mode(0, 115200);
    _regs->write<unsigned>(CR, Ctrl::Rxen | Ctrl::Txen);
    return true;
  }

  bool Uart_cadence::enable_rx_irq(bool enable)
  {
    _regs->write<unsigned>(RXWM, 1);
    _regs->write<unsigned>(ISR, ~0U);
    _regs->write<unsigned>(IER, enable ? IXR_RXOVR : 0);
    return true;
  }

  void Uart_cadence::shutdown()
  {
    _regs->write<unsigned>(CR, Ctrl::Rxdis | Ctrl::Txdis);
  }

  bool Uart_cadence::change_mode(Transfer_mode, Baud_rate r)
  {
    unsigned div = 4;
    _regs->write<unsigned>(Baud_rate_divider_reg0, div);
    _regs->write<unsigned>(BAUDGEN, _base_rate / r / (div + 1));
    _regs->write<unsigned>(MR, 0x20); // 8N1
    return true;
  }

  int Uart_cadence::get_char(bool blocking) const
  {
    while (!char_avail())
      if (!blocking)
        return -1;

    _regs->write<unsigned>(ISR, IXR_RXOVR);
    return _regs->read<unsigned>(FIFO);
  }

  int Uart_cadence::char_avail() const
  {
    return !(_regs->read<unsigned>(SR) & IXR_RXEMPTY);
  }

  void Uart_cadence::out_char(char c) const
  {
    // check for some free fifo space
    Poll_timeout_counter i(3000000);
    while (i.test(_regs->read<unsigned>(SR) & IXR_TXFULL))
      ;

    _regs->write<unsigned>(FIFO, c);
  }

  int Uart_cadence::write(char const *s, unsigned long count) const
  {
    unsigned long c = count;
    while (c--)
      out_char(*s++);

    return count;
  }

  void Uart_cadence::irq_ack()
  {
    _regs->write<unsigned>(ISR, IXR_RXOVR);
  }
};
