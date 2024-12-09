/*
 * Copyright (C) 2018, 2023 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
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

    // RX FIFO mode does not work together with enabled receive IRQ: The IRQ is
    // triggered when the UARTSR_DRF/RFE bit is set.
    // - This works in buffered mode: UARTSR_DRF is set when number of bytes
    //   programmed in RDFL have been received.
    // - This does NOT work in FIFO mode: UARTSR_RFE is set when the RX FIFO is
    //   empty.
    // Therefore we enable FIFO mode only if the receive IRQ is disabled.
    Fifo_rx_enabled = 1,

    Fifo_tx_enabled = 0,

    UARTCR_UART     = 1 <<  0, // 1=UART mode, 0=LIN mode
    UARTCR_WL0      = 1 <<  1, // word length in UART mode
    UARTCR_PCE      = 1 <<  2, // parity transmit check enable
    UARTCR_PC0      = 1 <<  3, // parity control
    UARTCR_TXEN     = 1 <<  4, // transmitter enable
    UARTCR_RXEN     = 1 <<  5, // receiver enable
    UARTCR_PC1      = 1 <<  6, // parity control
    UARTCR_WL1      = 1 <<  7, // word length in UART mode
    UARTCR_TFBM     = 1 <<  8, // TX FIFO enabled
    UARTCR_RFBM     = 1 <<  9, // RX FIFO enabled

    // RX buffered mode (UARTCR_RFBM=0)
    UARTCR_RDFL_1_byte  = 0 << 10, // RX buffer has 1 byte
    UARTCR_RDFL_2_bytes = 1 << 10, // RX buffer has 2 bytes
    UARTCR_RDFL_3_bytes = 2 << 10, // RX buffer has 3 bytes
    UARTCR_RDFL_4_bytes = 3 << 10, // RX buffer has 4 bytes
    // RX buffered mode (UARTCR_TFBM=0)
    UARTCR_TDFL_1_byte  = 0 << 13, // TX buffer has 1 byte
    UARTCR_TDFL_2_bytes = 1 << 13, // TX buffer has 2 bytes
    UARTCR_TDFL_3_bytes = 2 << 13, // TX buffer has 3 bytes
    UARTCR_TDFL_4_bytes = 3 << 13, // TX buffer has 4 bytes
    // RX FIFO mode (UARTCR_RFBM=1)
    UARTCR_RFC_mask     = 7 << 10, // number of bytes in RX FIFO (ro)
    UARTCR_RFC_empty    = 0 << 10, // RX FIFO is empty
    UARTCR_RFC_1_byte   = 1 << 10, // RX FIFO filled with 1 byte
    UARTCR_RFC_2_bytes  = 2 << 10, // RX FIFO filled with 2 bytes
    UARTCR_RFC_3_bytes  = 3 << 10, // RX FIFO filled with 3 bytes
    UARTCR_RFC_4_bytes  = 4 << 10, // RX FIFO filled with 4 bytes
    // TX FIFO mode (UARTCR_TFBM=1)
    UARTCR_TFC_mask     = 7 << 13, // number of bytes in TX FIFO (ro)
    UARTCR_TFC_empty    = 0 << 13, // TX FIFO is empty
    UARTCR_TFC_1_byte   = 1 << 13, // TX FIFO filled with 1 byte
    UARTCR_TFC_2_bytes  = 2 << 13, // TX FIFO filled with 2 bytes
    UARTCR_TFC_3_bytes  = 3 << 13, // TX FIFO filled with 3 bytes
    UARTCR_TFC_4_bytes  = 3 << 13, // TX FIFO filled with 4 bytes

    UARTCR_NEF      = 0 << 20, // number of expected frames

    UARTSR_DTF    = 1 << 1, // buffer: data transmission completed flag
    UARTSR_TFF    = 1 << 1, // FIFO: TX FIFO full
    UARTSR_DRF    = 1 << 2, // buffer: data reception completed flag
    UARTSR_RFE    = 1 << 2, // FIFO: RX FIFO empty
    UARTSR_TO     = 1 << 3, // timeout occurred
    UARTSR_RFNE   = 1 << 4, // FIFO: RX FIFO not empty
    UARTSR_BOF    = 1 << 7, // FIFO/buffer overrun flag
    UARTSR_RMB    = 1 << 9, // buffer ready to read by software if set
  };

  bool Uart_linflex::startup(Io_register_block const *regs)
  {
    _regs = regs;

    _regs->write<unsigned>(LINCR1, LINCR1_INIT | LINCR1_MME | LINCR1_BF);
    while ((_regs->read<unsigned>(LINSR) & LINSR_LINS) != LINSR_LINS_INITMODE)
      ;

    _regs->write<unsigned>(UARTCR, UARTCR_UART);

    set_uartcr(true);
    _regs->write<unsigned>(UARTSR, UARTSR_DRF | UARTSR_RMB);

    _regs->clear<unsigned>(LINCR1, LINCR1_INIT);

    return true;
  }

  bool Uart_linflex::is_tx_fifo_enabled() const
  {
    return _regs->read<unsigned>(UARTCR) & UARTCR_TFBM;
  }

  bool Uart_linflex::is_rx_fifo_enabled() const
  {
    return _regs->read<unsigned>(UARTCR) & UARTCR_RFBM;
  }

  void Uart_linflex::set_uartcr(bool fifo_rx_enable)
  {
    unsigned v =   UARTCR_UART  // UART mode (in contrast to LIN mode)
                 | UARTCR_WL0   // word length: 8 bits
                 | UARTCR_PC0   // parity handling
                 | UARTCR_PC1   // parity handling
                 | UARTCR_TXEN  // transmit enabled
                 | UARTCR_RXEN  // receive enabled
                 | UARTCR_NEF;  // number of expected frames

    if (fifo_rx_enable && Fifo_rx_enabled)
      v |= UARTCR_RFBM;         // RX FIFO enabled
    else
      v |= UARTCR_RDFL_1_byte;  // 1 byte in RX buffer

    if (Fifo_tx_enabled)
      v |= UARTCR_TFBM;         // TX FIFO enabled
    else
      v |= UARTCR_TDFL_1_byte;  // 1 byte in TX buffer

    _regs->write<unsigned>(UARTCR, v);
  }

  bool Uart_linflex::enable_rx_irq(bool enable)
  {
    _regs->set<unsigned>(LINCR1, LINCR1_INIT);
    while ((_regs->read<unsigned>(LINSR) & LINSR_LINS) != LINSR_LINS_INITMODE)
      ;

    if (enable)
      {
        if (Fifo_rx_enabled)
          set_uartcr(false);
        _regs->set<unsigned>(LINIER, LINIER_DRIE);
      }
    else
      {
        if (Fifo_rx_enabled)
          set_uartcr(true);
        _regs->clear<unsigned>(LINIER, LINIER_DRIE);
      }

    _regs->clear<unsigned>(LINCR1, LINCR1_INIT);

    return true;
  }

  void Uart_linflex::shutdown()
  {
  }

  bool Uart_linflex::change_mode(Transfer_mode, Baud_rate)
  {
    return true;
  }

  int Uart_linflex::get_char(bool blocking) const
  {
    while (!char_avail())
      if (!blocking)
        return -1;

    unsigned c;
    if (is_rx_fifo_enabled())
      {
        unsigned sr = _regs->read<unsigned>(UARTSR);
        _regs->write<unsigned>(UARTSR, sr);
        c = _regs->read<unsigned char>(BDRM);
      }
    else
      {
        unsigned sr = _regs->read<unsigned>(UARTSR);
        c = _regs->read<unsigned char>(BDRM);
        _regs->write<unsigned>(UARTSR, sr);
      }
    return c;
  }

  int Uart_linflex::char_avail() const
  {
    unsigned sr = _regs->read<unsigned>(UARTSR);
    sr &= (UARTSR_TO | UARTSR_BOF);
    if (sr)
      _regs->write<unsigned>(UARTSR, sr);
    if (is_rx_fifo_enabled())
      // UARTSR_RFNE=1: RX FIFO not empty.
      return (_regs->read<unsigned>(UARTSR) & UARTSR_RFNE);
    else
      // UARTSR_RMB=1: Buffer ready to read.
      return _regs->read<unsigned>(UARTSR) & UARTSR_RMB;
  }

  int Uart_linflex::tx_avail() const
  {
    if (is_tx_fifo_enabled())
      // UARTSR_TFF=1: TX FIFO full.
      return !(_regs->read<unsigned>(UARTSR) & UARTSR_TFF);
    else
      // UARTSR_DTF indicating that the character was transmitted was
      // immediately cleared in out_char() after transmission.
      return true;
  }

  void Uart_linflex::wait_tx_done() const
  {
    if (is_tx_fifo_enabled())
      {
        Poll_timeout_counter i(3000000);
        // FIFO: This is not 100% reliable, it might still happen that not all
        // characters are send when UARTCR_TFC signals the TX buffer is empty.
        while (i.test((_regs->read<unsigned>(UARTCR) & UARTCR_TFC_mask)
                      != UARTCR_TFC_empty))
          ;
      }
    else
      {
        // Already done in out_char() after writing a single character.
      }
  }

  void Uart_linflex::out_char(char c) const
  {
    _regs->write<unsigned char>(BDRL, c);

    if (is_tx_fifo_enabled())
      {
        // tx_avail() will return false as long as the TX FIFO is full.
      }
    else
      {
        Poll_timeout_counter i(3000000);
        // Buffer mode: UARTSR_DTF=1: Data transmission completed.
        while (i.test(!(_regs->read<unsigned>(UARTSR) & UARTSR_DTF)))
          ;
        _regs->write<unsigned>(UARTSR, UARTSR_DTF);
      }
  }

  int Uart_linflex::write(char const *s, unsigned long count, bool blocking) const
  {
    return generic_write<Uart_linflex>(s, count, blocking);
  }
}
