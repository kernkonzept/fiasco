#include "uart_s3c2410.h"
#include "poll_timeout_counter.h"

namespace L4
{
  enum
  {
    Type_24xx, Type_64xx, Type_s5pv210,
  };

  enum
  {
    ULCON   = 0x0,  // line control register
    UCON    = 0x4,  // control register
    UFCON   = 0x8,  // FIFO control register
    UMCON   = 0xc,  // modem control register
    UTRSTAT = 0x10, // Tx/Rx status register
    UERSTAT = 0x14, // Rx error status register
    UFSTAT  = 0x18, // FIFO status register
    UMSTAT  = 0x1c, // modem status register
    UTXH    = 0x20, // transmit buffer register (little endian, 0x23 for BE)
    URXH    = 0x24, // receive buffer register (little endian, 0x27 for BE)
    UBRDIV  = 0x28, // baud rate divisor register
    UFRACVAL= 0x2c,
    // 64xx++
    UINTP   = 0x30, // interrupt pending register
    UINTSP  = 0x34, // interrupt source pending register
    UINTM   = 0x38, // interrupt mask register


    ULCON_8N1_MODE = 0x3,

    UCON_MODE_RECEIVE_IRQ_POLL  = 1 << 0,
    UCON_MODE_TRANSMIT_IRQ_POLL = 1 << 2,
    UCON_SEND_BREAK_SIGNAL      = 1 << 4,
    UCON_LOOPBACK_MODE          = 1 << 5,
    UCON_RX_ERROR_STATUS_IRQ_EN = 1 << 6,
    UCON_RX_TIME_OUT_EN         = 1 << 7,
    UCON_MODE =   UCON_MODE_RECEIVE_IRQ_POLL
                | UCON_MODE_TRANSMIT_IRQ_POLL
                | UCON_RX_TIME_OUT_EN,

    UFCON_ENABLE        = 1 << 0, // enable fifo
    UFCON_RX_FIFO_RESET = 1 << 1, // Rx Fifo reset
    UFCON_TX_FIFO_RESET = 1 << 2, // Tx Fifo reset

    UMCON_AFC = 1 << 4,

    UTRSTAT_Rx_RDY = 1 << 0,
    UTRSTAT_Tx_RDY = 1 << 1,

    UINT_RXD   = 1 << 0,
    UINT_ERROR = 1 << 1,
    UINT_TXD   = 1 << 2,
    UINT_MODEM = 1 << 3,

    // 2410
    UFSTAT_2410_Rx_COUNT_MASK = 0x00f,
    UFSTAT_2410_Tx_COUNT_MASK = 0x0f0,
    UFSTAT_2410_RxFULL        = 0x100,
    UFSTAT_2410_TxFULL        = 0x200,

    // 64xx
    UFSTAT_64XX_Rx_COUNT_MASK = 0x003f,
    UFSTAT_64XX_Tx_COUNT_MASK = 0x3f00,
    UFSTAT_64XX_RxFULL        = 0x0040,
    UFSTAT_64XX_TxFULL        = 0x4000,

    // s5pv210
    UFSTAT_S5PV210_Rx_COUNT_MASK = 0x000000ff,
    UFSTAT_S5PV210_Tx_COUNT_MASK = 0x00ff0000,
    UFSTAT_S5PV210_RxFULL        = 0x00000100,
    UFSTAT_S5PV210_TxFULL        = 0x01000000,
  };


  void Uart_s3c::fifo_reset()
  {
    _regs->write<unsigned int>(UFCON, UFCON_RX_FIFO_RESET | UFCON_TX_FIFO_RESET);
    Poll_timeout_counter i(3000000);
    while (i.test(_regs->read<unsigned int>(UFCON) & (UFCON_RX_FIFO_RESET | UFCON_TX_FIFO_RESET)))
      ;
  }


  bool Uart_s3c::startup(Io_register_block const *regs)
  {
    _regs = regs;

    fifo_reset();
#if 0
    _regs->write<unsigned int>(UMCON, 0);
#endif

    _regs->write<unsigned int>(ULCON, ULCON_8N1_MODE);
    _regs->write<unsigned int>(UCON,  UCON_MODE);
    _regs->write<unsigned int>(UFCON, UFCON_ENABLE);

    switch (type())
      {
      case Type_24xx:
        break;
      case Type_64xx:
      case Type_s5pv210:
        _regs->write<unsigned int>(UINTM, ~UINT_RXD); // mask all but receive irq
        _regs->write<unsigned int>(UINTP, ~0); // clear all pending irqs
        break;
      }
#if 0
    _regs->write<unsigned int>(UBRDIV, 0x23);
#endif

    return true;
  }

  void Uart_s3c::shutdown()
  {
    // more
  }

  bool Uart_s3c::change_mode(Transfer_mode, Baud_rate r)
  {
    if (r != 115200)
      return false;

#if 0
    _regs->write<unsigned int>(ULCON, ULCON_8N1_MODE);
    _regs->write<unsigned int>(UCON,  UCON_MODE);
    _regs->write<unsigned int>(UFCON, 1);

    _regs->write<unsigned int>(UBRDIV, 0x23);
#endif

    return true;
  }

  int Uart_s3c::get_char(bool blocking) const
  {
    while (!char_avail())
      if (!blocking)
        return -1;

    _regs->read<unsigned int>(UFCON);
    int c = _regs->read<unsigned int>(URXH) & 0xff;
    ack_rx_irq();
    return c;
  }

  int Uart_s3c::char_avail() const
  {
    return is_rx_fifo_non_empty();
  }

  void Uart_s3c::out_char(char c) const
  {
    wait_for_non_full_tx_fifo();
    _regs->write<unsigned int>(UTXH, c);
  }

  int Uart_s3c::write(char const *s, unsigned long count) const
  {
    unsigned long c = count;
    while (c--)
      out_char(*s++);
    wait_for_empty_tx_fifo();

    return count;
  }

  // -----------------------

  void Uart_s3c2410::wait_for_empty_tx_fifo() const
  {
    Poll_timeout_counter i(3000000);
    while (i.test(_regs->read<unsigned int>(UFSTAT) & (UFSTAT_2410_Tx_COUNT_MASK | UFSTAT_2410_TxFULL)))
      ;
  }

  void Uart_s3c2410::wait_for_non_full_tx_fifo() const
  {
    Poll_timeout_counter i(3000000);
    while (i.test(_regs->read<unsigned int>(UFSTAT) & UFSTAT_2410_TxFULL))
      ;
  }

  unsigned Uart_s3c2410::is_rx_fifo_non_empty() const
  {
    return _regs->read<unsigned int>(UFSTAT) & (UFSTAT_2410_Rx_COUNT_MASK | UFSTAT_2410_RxFULL);
  }

  void Uart_s3c2410::auto_flow_control(bool on)
  {
    _regs->write<unsigned int>(UMCON, (_regs->read<unsigned int>(UMCON) & ~UMCON_AFC) | (on ? UMCON_AFC : 0));
  }

  // -----------------------

  void Uart_s3c64xx::wait_for_empty_tx_fifo() const
  {
    Poll_timeout_counter i(3000000);
    while (i.test(_regs->read<unsigned int>(UFSTAT) & (UFSTAT_64XX_Tx_COUNT_MASK | UFSTAT_64XX_TxFULL)))
      ;
  }

  void Uart_s3c64xx::wait_for_non_full_tx_fifo() const
  {
    Poll_timeout_counter i(3000000);
    while (i.test(_regs->read<unsigned int>(UFSTAT) & UFSTAT_64XX_TxFULL))
      ;
  }

  unsigned Uart_s3c64xx::is_rx_fifo_non_empty() const
  {
    return _regs->read<unsigned int>(UFSTAT) & (UFSTAT_64XX_Rx_COUNT_MASK | UFSTAT_64XX_RxFULL);
  }

  void Uart_s3c64xx::ack_rx_irq() const
  {
    _regs->write<unsigned int>(UINTP, UINT_RXD);
  }

  // -----------------------

  void Uart_s5pv210::wait_for_empty_tx_fifo() const
  {
    Poll_timeout_counter i(3000000);
    while (i.test(_regs->read<unsigned int>(UFSTAT) & (UFSTAT_S5PV210_Tx_COUNT_MASK | UFSTAT_S5PV210_TxFULL)))
      ;
  }

  void Uart_s5pv210::wait_for_non_full_tx_fifo() const
  {
    Poll_timeout_counter i(3000000);
    while (i.test(_regs->read<unsigned int>(UFSTAT) & UFSTAT_S5PV210_TxFULL))
      ;
  }

  unsigned Uart_s5pv210::is_rx_fifo_non_empty() const
  {
    return _regs->read<unsigned int>(UFSTAT) & (UFSTAT_S5PV210_Rx_COUNT_MASK | UFSTAT_S5PV210_RxFULL);
  }

  void Uart_s5pv210::ack_rx_irq() const
  {
    _regs->write<unsigned int>(UINTP, UINT_RXD);
  }

  void Uart_s5pv210::save(Save_block *b) const
  {
    b->ubrdiv   = _regs->read<unsigned>(UBRDIV);
    b->uintm    = _regs->read<unsigned>(UINTM);
    b->ufracval = _regs->read<unsigned>(UFRACVAL);
    b->umcon    = _regs->read<unsigned>(UMCON);
    b->ufcon    = _regs->read<unsigned>(UFCON);
    b->ucon     = _regs->read<unsigned>(UCON);
    b->ulcon    = _regs->read<unsigned>(ULCON);
  }

  void Uart_s5pv210::restore(Save_block const *b) const
  {
    _regs->write<unsigned>(UINTM,    b->uintm);
    _regs->write<unsigned>(ULCON,    b->ulcon);
    _regs->write<unsigned>(UCON,     b->ucon);
    _regs->write<unsigned>(UFCON,    b->ufcon);
    _regs->write<unsigned>(UMCON,    b->umcon);
    _regs->write<unsigned>(UBRDIV,   b->ubrdiv);
    _regs->write<unsigned>(UFRACVAL, b->ufracval);
  }
};

