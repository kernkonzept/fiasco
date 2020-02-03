/*!
 * \file   uart_sa1000.cc
 * \brief  SA1000 Uart
 *
 * \date   2008-01-02
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *         Alexander Warg <alexander.warg@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2008-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "uart_sa1000.h"
#include "poll_timeout_counter.h"

namespace L4
{
  enum {
    PAR_NONE = 0x00,
    PAR_EVEN = 0x03,
    PAR_ODD  = 0x01,
    DAT_5    = (unsigned)-1,
    DAT_6    = (unsigned)-1,
    DAT_7    = 0x00,
    DAT_8    = 0x08,
    STOP_1   = 0x00,
    STOP_2   = 0x04,

    MODE_8N1 = PAR_NONE | DAT_8 | STOP_1,
    MODE_7E1 = PAR_EVEN | DAT_7 | STOP_1,

    // these two values are to leave either mode
    // or baud rate unchanged on a call to change_mode
    MODE_NC  = 0x1000000,
    BAUD_NC  = 0x1000000,
  };

  enum {
    UTCR0 = 0x00,
    UTCR1 = 0x04,
    UTCR2 = 0x08,
    UTCR3 = 0x0c,
    UTCR4 = 0x10,
    UTDR  = 0x14,
    UTSR0 = 0x1c,
    UTSR1 = 0x20,


    UTCR0_PE  = 0x01,
    UTCR0_OES = 0x02,
    UTCR0_SBS = 0x04,
    UTCR0_DSS = 0x08,
    UTCR0_SCE = 0x10,
    UTCR0_RCE = 0x20,
    UTCR0_TCE = 0x40,

    UTCR3_RXE = 0x01,
    UTCR3_TXE = 0x02,
    UTCR3_BRK = 0x04,
    UTCR3_RIE = 0x08,
    UTCR3_TIE = 0x10,
    UTCR3_LBM = 0x20,


    UTSR0_TFS = 0x01,
    UTSR0_RFS = 0x02,
    UTSR0_RID = 0x04,
    UTSR0_RBB = 0x08,
    UTSR0_REB = 0x10,
    UTSR0_EIF = 0x20,

    UTSR1_TBY = 0x01,
    UTSR1_RNE = 0x02,
    UTSR1_TNF = 0x04,
    UTSR1_PRE = 0x08,
    UTSR1_FRE = 0x10,
    UTSR1_ROR = 0x20,

    UARTCLK = 3686400,
  };

  bool Uart_sa1000::startup(Io_register_block const *regs)
  {
    _regs = regs;
    _regs->write<unsigned int>(UTSR0, ~0U); // clear pending status bits
    _regs->write<unsigned int>(UTCR3, UTCR3_RXE | UTCR3_TXE); //enable transmitter and receiver
    return true;
  }

  void Uart_sa1000::shutdown()
  {
    _regs->write<unsigned int>(UTCR3, 0);
  }

  bool Uart_sa1000::change_mode(Transfer_mode m, Baud_rate baud)
  {
    unsigned old_utcr3, quot;
    //proc_status st;

    if (baud == (Baud_rate)-1)
      return false;
    if (baud != BAUD_NC && (baud>115200 || baud<96))
      return false;
    if (m == (Transfer_mode)-1)
      return false;

    //st = proc_cli_save();
    old_utcr3 = _regs->read<unsigned int>(UTCR3);
    _regs->write<unsigned int>(UTCR3, (old_utcr3 & ~(UTCR3_RIE|UTCR3_TIE)));
    //proc_sti_restore(st);

    Poll_timeout_counter i(3000000);
    while (i.test(_regs->read<unsigned int>(UTSR1) & UTSR1_TBY))
      ;

    /* disable all */
    _regs->write<unsigned int>(UTCR3, 0);

    /* set parity, data size, and stop bits */
    if(m != MODE_NC)
      _regs->write<unsigned int>(UTCR0, m & 0x0ff);

    /* set baud rate */
    if(baud!=BAUD_NC)
      {
	quot = (UARTCLK / (16*baud)) -1;
	_regs->write<unsigned int>(UTCR1, (quot & 0xf00) >> 8);
	_regs->write<unsigned int>(UTCR2, quot & 0x0ff);
      }

    _regs->write<unsigned int>(UTSR0, (unsigned)-1);
    _regs->write<unsigned int>(UTCR3, old_utcr3);
    return true;

  }

  int Uart_sa1000::get_char(bool blocking) const
  {
    int ch;
    unsigned long old_utcr3 = _regs->read<unsigned int>(UTCR3);
    _regs->write<unsigned int>(UTCR3, old_utcr3 & ~(UTCR3_RIE|UTCR3_TIE));

    while (!char_avail())
      if (!blocking)
	return -1;

    ch = _regs->read<unsigned int>(UTDR);
    _regs->write<unsigned int>(UTCR3, old_utcr3);
    return ch;

  }

  int Uart_sa1000::char_avail() const
  {
    return !!(_regs->read<unsigned int>(UTSR1) & UTSR1_RNE);
  }

  void Uart_sa1000::out_char(char c) const
  {
    // do UTCR3 thing here as well?
    Poll_timeout_counter i(3000000);
    while(i.test(!(_regs->read<unsigned int>(UTSR1) & UTSR1_TNF)))
      ;
    _regs->write<unsigned int>(UTDR, c);
  }

  int Uart_sa1000::write(char const *s, unsigned long count) const
  {
    unsigned old_utcr3;
    unsigned i;

    old_utcr3 = _regs->read<unsigned int>(UTCR3);
    _regs->write<unsigned int>(UTCR3, (old_utcr3 & ~(UTCR3_RIE | UTCR3_TIE)) | UTCR3_TXE );

    /* transmission */
    for (i = 0; i < count; i++)
      out_char(s[i]);

    /* wait till everything is transmitted */
    Poll_timeout_counter cnt(3000000);
    while (cnt.test(_regs->read<unsigned int>(UTSR1) & UTSR1_TBY))
      ;

    _regs->write<unsigned int>(UTCR3, old_utcr3);
    return count;
  }
};
