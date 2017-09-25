#include "uart_pl011.h"
#include "poll_timeout_counter.h"

namespace L4
{
  enum {
    UART011_RXIM = 1 << 4,
    UART011_TXIM = 1 << 5,
    UART011_RTIM = 1 << 6,
    UART011_FEIM = 1 << 7,
    UART011_PEIM = 1 << 8,
    UART011_BEIM = 1 << 9,
    UART011_OEIM = 1 << 10,

    UART011_RXIS = 1 << 4,
    UART011_RTIS = 1 << 6,

    UART011_RXIC = 1 << 4,
    UART011_RTIC = 1 << 6,

    UART01x_CR_UARTEN = 1, // UART enable
    UART011_CR_LBE    = 0x080, // loopback enable
    UART011_CR_TXE    = 0x100, // transmit enable
    UART011_CR_RXE    = 0x200, // receive enable

    UART01x_FR_BUSY   = 0x008,
    UART01x_FR_RXFE   = 0x010,
    UART01x_FR_TXFF   = 0x020,

    UART01x_LCRH_PEN    = 0x02, // parity enable
    UART01x_LCRH_FEN    = 0x10, // FIFO enable
    UART01x_LCRH_WLEN_8 = 0x60,

    UART01x_DR   = 0x00,
    UART011_ECR  = 0x04,
    UART01x_FR   = 0x18,
    UART011_IBRD = 0x24,
    UART011_FBRD = 0x28,
    UART011_LCRH = 0x2c,
    UART011_CR   = 0x30,
    UART011_IMSC = 0x38,
    UART011_MIS  = 0x40,
    UART011_ICR  = 0x44,

    Default_baud = 115200,
  };

  bool Uart_pl011::startup(Io_register_block const *regs)
  {
    _regs = regs;
    _regs->write<unsigned int>(UART011_CR, UART01x_CR_UARTEN | UART011_CR_TXE | UART011_CR_RXE);
    unsigned fi_val = _freq * 4 / Default_baud;
    _regs->write<unsigned int>(UART011_FBRD, fi_val & 0x3f);
    _regs->write<unsigned int>(UART011_IBRD, fi_val >> 6);
    _regs->write<unsigned int>(UART011_LCRH, UART01x_LCRH_WLEN_8);
    _regs->write<unsigned int>(UART011_IMSC, 0);
    Poll_timeout_counter i(3000000);
    while (i.test() && _regs->read<unsigned int>(UART01x_FR) & UART01x_FR_BUSY)
      ;
    return true;
  }

  void Uart_pl011::shutdown()
  {
    _regs->write<unsigned int>(UART011_IMSC, 0);
    _regs->write<unsigned int>(UART011_ICR, 0xffff);
    _regs->write<unsigned int>(UART011_CR, 0);
  }

  bool Uart_pl011::enable_rx_irq(bool enable)
  {
    unsigned long mask = UART011_RXIM | UART011_RTIM;

    _regs->write<unsigned int>(UART011_ICR, 0xffff & ~mask);
    _regs->write<unsigned int>(UART011_ECR, 0xff);
    if (enable)
      _regs->write<unsigned int>(UART011_IMSC, _regs->read<unsigned int>(UART011_IMSC) | mask);
    else
      _regs->write<unsigned int>(UART011_IMSC, _regs->read<unsigned int>(UART011_IMSC) & ~mask);
    return true;
  }

  bool Uart_pl011::change_mode(Transfer_mode, Baud_rate r)
  {
    unsigned long old_cr = _regs->read<unsigned int>(UART011_CR);
    _regs->write<unsigned int>(UART011_CR, 0);

    unsigned fi_val = _freq * 4 / r;
    _regs->write<unsigned int>(UART011_FBRD, fi_val & 0x3f);
    _regs->write<unsigned int>(UART011_IBRD, fi_val >> 6);
    _regs->write<unsigned int>(UART011_LCRH, UART01x_LCRH_WLEN_8 | UART01x_LCRH_FEN);

    _regs->write<unsigned int>(UART011_CR, old_cr);

    return true;
  }

  int Uart_pl011::get_char(bool blocking) const
  {
    while (!char_avail())
      if (!blocking)
        return -1;

    //_regs->write(UART011_ICR, UART011_RXIC | UART011_RTIC);

    int c = _regs->read<unsigned int>(UART01x_DR);
    _regs->write<unsigned int>(UART011_ECR, 0xff);
    return c;
  }

  int Uart_pl011::char_avail() const
  {
    return !(_regs->read<unsigned int>(UART01x_FR) & UART01x_FR_RXFE);
  }

  void Uart_pl011::out_char(char c) const
  {
    Poll_timeout_counter i(3000000);
    while (i.test(_regs->read<unsigned int>(UART01x_FR) & UART01x_FR_TXFF))
      ;
    _regs->write<unsigned int>(UART01x_DR,c);
  }

  int Uart_pl011::write(char const *s, unsigned long count) const
  {
    unsigned long c = count;
    while (c--)
      out_char(*s++);

    Poll_timeout_counter i(3000000);
    while (i.test(_regs->read<unsigned int>(UART01x_FR) & UART01x_FR_BUSY))
      ;

    return count;
  }
};
