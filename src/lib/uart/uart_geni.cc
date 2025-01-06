/*
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 * Author(s): Stephan Gerhold <stephan.gerhold@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/**
 * \file
 * Driver for Qualcomm "Generic Interface (GENI) Serial Engine (SE)",
 * used in most Qualcomm platforms starting from the Snapdragon 845.
 *
 * At the moment the driver implements the minimum necessary to get serial
 * output/input and interrupts. Further optimizations would be possible,
 * e.g. by writing/reading 4 chars at a time when possible. The packing
 * format for the TX/RX FIFO can be adjusted arbitrarily through the
 * TX/RX_PACKING_* registers.
 */

#include "uart_geni.h"
#include "poll_timeout_counter.h"

namespace L4
{
  enum
  {
    FORCE_DEFAULT_REG = 0x020,
    STATUS            = 0x040,

    BYTE_GRAN         = 0x254,
    TX_PACKING_CFG0   = 0x260,
    TX_PACKING_CFG1   = 0x264,
    UART_TX_TRANS_LEN = 0x270,
    RX_PACKING_CFG0   = 0x284,
    RX_PACKING_CFG1   = 0x288,

    M_CMD0            = 0x600,
    M_CMD_CTRL        = 0x604,
    M_IRQ_STATUS      = 0x610,
    M_IRQ_EN          = 0x614,
    M_IRQ_CLEAR       = 0x618,
    S_CMD0            = 0x630,
    S_CMD_CTRL        = 0x634,
    S_IRQ_STATUS      = 0x640,
    S_IRQ_EN          = 0x644,
    S_IRQ_CLEAR       = 0x648,

    TX_FIFOn          = 0x700,
    RX_FIFOn          = 0x780,
    TX_FIFO_STATUS    = 0x800,
    RX_FIFO_STATUS    = 0x804,

    IRQ_EN            = 0xe1c,

    STATUS_M_GENI_CMD_ACTIVE = 1 << 0,
    STATUS_S_GENI_CMD_ACTIVE = 1 << 12,
    M_CMD_CTRL_ABORT         = 1 << 1,
    M_CMD0_OP_UART_START_TX  = 0x1 << 27,
    S_CMD_CTRL_ABORT         = 1 << 1,
    S_CMD0_OP_UART_START_RX  = 0x1 << 27,
    S_IRQ_RX_FIFO_LAST       = 1 << 27,
    TX_FIFO_STATUS_WC        = 0xfffffff,
    RX_FIFO_STATUS_WC        = 0x1ffffff,
    IRQ_EN_M                 = 1 << 2,
    IRQ_EN_S                 = 1 << 3,

    // 4x8 packing (TX_FIFOn accepts up to 4 chars in a single 32-bit write)
    // NOTE: For simplicity all bytes are written separately at the moment
    TX_PACKING_CFG0_4BYTE    = 0x4380e,
    TX_PACKING_CFG1_4BYTE    = 0xc3e0e,
    // 1x8 packing (a 32-bit read of RX_FIFOn always returns a single char)
    RX_PACKING_CFG0_1BYTE    = 0x0000f,
    RX_PACKING_CFG1_1BYTE    = 0x00000,
  };

  bool Uart_geni::startup(Io_register_block const *regs)
  {
    _regs = regs;

    shutdown();

    // Configure FIFO packing
    _regs->write32(TX_PACKING_CFG0, TX_PACKING_CFG0_4BYTE);
    _regs->write32(TX_PACKING_CFG1, TX_PACKING_CFG1_4BYTE);
    _regs->write32(RX_PACKING_CFG0, RX_PACKING_CFG0_1BYTE);
    _regs->write32(RX_PACKING_CFG1, RX_PACKING_CFG1_1BYTE);

    enable_rx_irq(false);
    _regs->write32(S_CMD0, S_CMD0_OP_UART_START_RX);
    return true;
  }

  void Uart_geni::shutdown()
  {
    wait_tx_done();

    // Abort RX and TX commands
    auto status = _regs->read32(STATUS);
    if (status & STATUS_M_GENI_CMD_ACTIVE)
      _regs->write32(M_CMD_CTRL, M_CMD_CTRL_ABORT);
    if (status & STATUS_S_GENI_CMD_ACTIVE)
      _regs->write32(S_CMD_CTRL, S_CMD_CTRL_ABORT);

    // Wait until commands are aborted
    Poll_timeout_counter i(3000000);
    while (_regs->read32(STATUS) &
           (STATUS_M_GENI_CMD_ACTIVE | STATUS_S_GENI_CMD_ACTIVE))
      ;

    // Force defaults on output pads
    _regs->write32(FORCE_DEFAULT_REG, 1);
  }

  bool Uart_geni::change_mode(Transfer_mode, Baud_rate)
  { return true; }

  bool Uart_geni::enable_rx_irq(bool enable)
  {
    if (!enable)
      {
        _regs->write32(IRQ_EN, 0);
        return true;
      }

    // Set up interrupts for secondary sequencer (used for RX)
    _regs->write32(S_IRQ_CLEAR, ~0U);
    _regs->write32(S_IRQ_EN, S_IRQ_RX_FIFO_LAST);
    _regs->write32(IRQ_EN, IRQ_EN_S);
    return true;
  }

  void Uart_geni::irq_ack()
  {
    // FIXME: Is RX_FIFO_LAST really the correct interrupt type here?
    _regs->write32(S_IRQ_CLEAR, S_IRQ_RX_FIFO_LAST);
  }

  int Uart_geni::get_char(bool blocking) const
  {
    while (!char_avail())
      if (!blocking)
        return -1;

    return _regs->read32(RX_FIFOn) & 0xff;
  }

  int Uart_geni::char_avail() const
  {
    return _regs->read32(RX_FIFO_STATUS) & RX_FIFO_STATUS_WC;
  }

  int Uart_geni::tx_avail() const
  {
    return !(_regs->read32(STATUS) & STATUS_M_GENI_CMD_ACTIVE);
  }

  void Uart_geni::wait_tx_done() const
  {
    Poll_timeout_counter i(3000000);
    while (i.test(_regs->read32(STATUS) & STATUS_M_GENI_CMD_ACTIVE))
      ;
  }

  void Uart_geni::out_char(char c) const
  {
    _regs->write32(UART_TX_TRANS_LEN, 1);
    _regs->write32(M_CMD0, M_CMD0_OP_UART_START_TX);
    _regs->write32(TX_FIFOn, c);
  }

  int Uart_geni::write(char const *s, unsigned long count, bool blocking) const
  {
    return generic_write<Uart_geni>(s, count, blocking);
  }
}
