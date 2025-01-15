/*
 * Copyright (C) 2008-2011 Technische Universität Dresden.
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "uart_imx.h"
#include "poll_timeout_counter.h"

namespace L4
{
  enum {
    URXD  = 0x00,   // Receiver
    UTXD  = 0x40,   // Transmitter
    UCR1  = 0x80,   // Control 1
    UCR2  = 0x84,   // Control 2
    UCR3  = 0x88,   // Control 3
    UCR4  = 0x8c,   // Control 4
    UFCR  = 0x90,   // FIFO Control
    USR1  = 0x94,   // Status 1
    USR2  = 0x98,   // Status 2
    UESC  = 0x9c,   // Escape Charater
    UTIM  = 0xa0,   // Escape Timer
    UBIR  = 0xa4,   // BRM Incremental Registers
    UBMR  = 0xa8,   // BRM Modulator Registers
    UBRC  = 0xac,   // Baud Rate Count
    ONEMS = 0xb0,   // One millisecond
    UTS   = 0xb4,   // Test


    UCR1_EN        = 1  <<  0, // Enable UART
    UCR1_RTSDEN    = 1  <<  5, // RTS Delta Interrupt Enable
    UCR1_RRDYEN    = 1  <<  9, // Receiver Ready Interrupt Enable

    UCR2_SRST      = 1  <<  0, // Software Reset
    UCR2_RXEN      = 1  <<  1, // Receiver Enable
    UCR2_TXEN      = 1  <<  2, // Transmitter Enable
    UCR2_WS        = 1  <<  5, // 8-bit character length
    UCR2_STPB      = 1  <<  6, // 0 = 1 Stop bit, 1 = 2 stop bits
    UCR2_PROE      = 1  <<  7, // 0 = even parity, 1 = odd parity
    UCR2_PREN      = 1  <<  8, // Parity enable
    UCR2_RTEC_MASK = 3  <<  9, // Request to Send Edge Control mask
    UCR2_RTEC_RISI = 0  <<  9, // Trigger IRQ on rising edge
    UCR2_RTEC_FALL = 1  <<  9, // Trigger IRQ on falling edge
    UCR2_RTEC_ANY  = 2  <<  9, // Trigger IRQ on any edge
    UCR2_ESCEN     = 1  << 11, // Escape enable
    UCR2_CTS       = 1  << 12, // Clear to Send: 0 = pin is high (inactive), 1 = pin is low (active)
    UCR2_CTSC      = 1  << 13, // CTS Pin Control: 0 = pin controlled by CTS bit, 1 = pin controlled by the receiver
    UCR2_IRTS      = 1  << 14, // Ignore RTS pin
    UCR2_ESCI      = 1  << 15, // Escape Sequence Interrupt Enable

    UCR3_ACIEN     = 1  <<  0, // Autobaud Counter Interrupt enable
    UCR3_INVT      = 1  <<  1, // Inverted Infrared Transmission
    UCR3_RXDMUXSEL = 1  <<  2, // RXD Muxed Input Selected: 0 = serial ist IPP_UART_RXD, IR is IPP_UART_RXD_IR, 1 = IPP_UART_RXD_MUX for both
    UCR3_AWAKEN    = 1  <<  3, // Asynchronous WAKE Interrupt Enable


    UCR4_DREN      = 1  <<  0, // Receive Data Ready Interrupt Enable
    UCR4_OREN      = 1  <<  1, // Receiver Overrun Interrupt enable
    UCR4_BKEN      = 1  <<  2, // BREAK Condition Dected Interrupt enable
    UCR4_TCEN      = 1  <<  3, // Transmit Complete Interrupt Enable
    UCR4_LPBYP     = 1  <<  4, // Low Power Bypass
    UCR4_IRSC      = 1  <<  5, // IR Special Case
    // Bit 6 is reserve, should be written as 0
    UCR4_WKEN      = 1  <<  7, // WAKE Interrupt Enable
    UCR4_ENIRI     = 1  <<  8, // Serial Infrared Interrupt Enable
    UCR4_INVR      = 1  <<  9, // Inverted Infrared Reception
    UCR4_CTSTL_32  = 32 << 10, // CTS Trigger Level

    UFCR_RXTL_MASK = 63 <<  0, // Receiver Trigger Level Mask
    UFCR_RXTL_1    = 1  <<  0, // Receiver Trigger Level: 1 char
    UFCR_RFDIV_2   = 4  <<  7, // Reference Frequency Divier: by 2
    UFCR_TXTL_MASK = 63 << 10, // Trasmitter Trigger Level: 8 chars
    UFCR_TXTL_8    = 8  << 10, // Trasmitter Trigger Level: 8 chars
    UFCR_TXTL_32   = 32 << 10, // Trasmitter Trigger Level: 32 chars

    USR1_AWAKE     = 1  <<  4, // Asynchronous WAKE Interrupt Flag
    USR1_TRDY      = 1  << 13, // Transmitter Ready

    USR2_RDR       = 1  <<  0, // Receive Data Ready
    USR2_ORE       = 1  <<  1, // Overrun Error
    USR2_BRCD      = 1  <<  2, // Break Condition Detected
    USR2_TXDC      = 1  <<  3, // Transmitter Complete
    USR2_TXFE      = 1  << 14, // Transmit Buffer FIFO Empty

  };

  bool Uart_imx::startup(Io_register_block const *regs)
  {
    _regs = regs;

    // 115200Baud, 8n1
    switch (_type)
      {
      case Type_imx21:
        _regs->write<unsigned int>(UBIR, 0x0344);
        _regs->write<unsigned int>(UBMR, 0x270f);
        break;
      case Type_imx35:
        _regs->write<unsigned int>(UBIR, 0xf);
        _regs->write<unsigned int>(UBMR, 0x1b2);
        break;
      case Type_imx51:
        _regs->write<unsigned int>(UBIR, 0xf);
        _regs->write<unsigned int>(UBMR, 0x120);
        break;
      case Type_imx6:
        _regs->write<unsigned int>(UBIR, 0xf);
        _regs->write<unsigned int>(UBMR, 0x15b);
        break;
      case Type_imx7:
        _regs->write<unsigned int>(UBIR, 0xf);
        _regs->write<unsigned int>(UBMR, 0x68);
        break;
      case Type_imx8:
        _regs->write<unsigned int>(UBIR, 0xf);
        _regs->write<unsigned int>(UBMR, 0x6c);
        break;
      }

    _regs->write<unsigned int>(UCR1, UCR1_EN);
    _regs->write<unsigned int>(UCR2, UCR2_SRST | UCR2_RXEN | UCR2_TXEN | UCR2_WS | UCR2_IRTS); // note: no IRQs enabled
    _regs->write<unsigned int>(UCR3, UCR3_RXDMUXSEL);
    _regs->write<unsigned int>(UCR4, UCR4_CTSTL_32);
    _regs->write<unsigned int>(UFCR, UFCR_TXTL_8 | UFCR_RFDIV_2 | UFCR_RXTL_1);

    return true;
  }

  void Uart_imx::shutdown()
  {
    _regs->write<unsigned int>(UCR1, 0); // Disable UART
  }

  bool Uart_imx::enable_rx_irq(bool enable)
  {
    if (enable)
      {
	_regs->write<unsigned int>(UCR2, _regs->read<unsigned int>(UCR2) | UCR2_RTEC_ANY);
	_regs->write<unsigned int>(UCR4, _regs->read<unsigned int>(UCR4) | UCR4_DREN);
      }
    else
      _regs->write<unsigned int>(UCR4, _regs->read<unsigned int>(UCR4) & ~UCR4_DREN);

    return true;
  }

  void Uart_imx6::irq_ack()
  {
    unsigned int usr1 = _regs->read<unsigned int>(USR1);

    if (usr1 & USR1_AWAKE)
      _regs->write<unsigned int>(USR1, USR1_AWAKE);
  }

  bool Uart_imx::change_mode(Transfer_mode, Baud_rate)
  { return true; }

  int Uart_imx::get_char(bool blocking) const
  {
    while (!char_avail())
      if (!blocking) return -1;

    return _regs->read<unsigned int>(URXD) & 0xff;
  }

  int Uart_imx::char_avail() const
  {
    return _regs->read<unsigned int>(USR2) & USR2_RDR;
  }

  int Uart_imx::tx_avail() const
  {
    return _regs->read<unsigned int>(USR1) & USR1_TRDY;
  }

  void Uart_imx::wait_tx_done() const
  {
    Poll_timeout_counter i(3000000);
    while (i.test(!(_regs->read<unsigned int>(USR2) & USR2_TXDC)))
      ;
  }

  void Uart_imx::out_char(char c) const
  {
    _regs->write<unsigned int>(UTXD, c);
  }

  int Uart_imx::write(char const *s, unsigned long count, bool blocking) const
  {
    return generic_write<Uart_imx>(s, count, blocking);
  }
};
