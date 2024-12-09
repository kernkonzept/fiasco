/*
 * Copyright (C) 2009-2012 Technische Universität Dresden.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "uart_omap35x.h"
#include "poll_timeout_counter.h"

namespace L4
{
  enum {
    DLL_REG  = 0x00,
    RHR_REG  = 0x00,
    THR_REG  = 0x00,
    IER_REG  = 0x04,
    DLH_REG  = 0x04,
    LCD_REG  = 0x08,
    LSR_REG  = 0x14,
    SSR_REG  = 0x44,
    SYSC_REG = 0x54,
    SYSS_REG = 0x58,

    LCD_REG_CHAR_LENGTH_5BIT       = 0 << 0,
    LCD_REG_CHAR_LENGTH_6BIT       = 1 << 0,
    LCD_REG_CHAR_LENGTH_7BIT       = 2 << 0,
    LCD_REG_CHAR_LENGTH_8BIT       = 3 << 0,
    LCD_REG_CHAR_NB_STOP_2         = 1 << 2,
    LCD_REG_CHAR_PARITY_EN         = 1 << 3,
    LCD_REG_CHAR_PARITY_TYPE1_EVEN = 1 << 4,

    LSR_REG_RX_FIFO_E              = 1 << 0, // at least one character in FIFO
    LSR_REG_TX_FIFO_E              = 1 << 5, // transmit hold register empty
    LSR_REG_TX_SR_E                = 1 << 6, // transmit+shift registers empty

    SSR_REG_TX_FIFO_FULL           = 1 << 0,

    SYSC_REG_SOFTRESET             = 1 << 1,

    SYSC_REG_RESETDONE             = 1 << 0,
  };


  bool Uart_omap35x::startup(Io_register_block const *regs)
  {
    _regs = regs;

    // Reset UART
    //_regs->write<unsigned int>(SYSC_REG, _regs->read<unsigned int>(SYSC_REG) | SYSC_REG_SOFTRESET);
    // Poll_timeout_counter i(3000000);
    //while (i.test(!(_regs->read<unsigned int>(SYSS_REG) & SYSC_REG_RESETDONE)))
    //  ;

    return true;
  }

  void Uart_omap35x::shutdown()
  {
  }

  bool Uart_omap35x::enable_rx_irq(bool enable)
  {
    _regs->write<unsigned int>(IER_REG, enable ? 1 : 0);
    return true;
  }

  bool Uart_omap35x::change_mode(Transfer_mode, Baud_rate r)
  {
    if (r != 115200)
      return false;

    return true;
  }

  int Uart_omap35x::get_char(bool blocking) const
  {
    while (!char_avail())
      if (!blocking)
	return -1;

    return _regs->read<unsigned int>(RHR_REG);
  }

  int Uart_omap35x::char_avail() const
  {
    return _regs->read<unsigned int>(LSR_REG) & LSR_REG_RX_FIFO_E;
  }

  int Uart_omap35x::tx_avail() const
  {
    return _regs->read<unsigned int>(LSR_REG) & LSR_REG_TX_FIFO_E;
  }

  void Uart_omap35x::wait_tx_done() const
  {
    Poll_timeout_counter i(3000000);
    while (i.test(!(_regs->read<unsigned char>(LSR_REG) & LSR_REG_TX_SR_E)))
      ;
  }

  void Uart_omap35x::out_char(char c) const
  {
    _regs->write<unsigned int>(THR_REG, c);
  }

  int Uart_omap35x::write(char const *s, unsigned long count, bool blocking) const
  {
    return generic_write<Uart_omap35x>(s, count, blocking);
  }
};
