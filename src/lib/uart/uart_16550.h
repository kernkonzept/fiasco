/*
 * (c) 2008-2012 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *               Alexander Warg <alexander.warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "uart_base.h"

namespace L4
{
  class Uart_16550 : public Uart
  {
  protected:
    enum Registers
    {
      TRB      = 0x00, // Transmit/Receive Buffer  (read/write)
      BRD_LOW  = 0x00, // Baud Rate Divisor LSB if bit 7 of LCR is set  (read/write)
      IER      = 0x01, // Interrupt Enable Register  (read/write)
      BRD_HIGH = 0x01, // Baud Rate Divisor MSB if bit 7 of LCR is set  (read/write)
      IIR      = 0x02, // Interrupt Identification Register  (read only)
      FCR      = 0x02, // 16550 FIFO Control Register  (write only)
      LCR      = 0x03, // Line Control Register  (read/write)
      MCR      = 0x04, // Modem Control Register  (read/write)
      LSR      = 0x05, // Line Status Register  (read only)
      MSR      = 0x06, // Modem Status Register  (read only)
      SPR      = 0x07, // Scratch Pad Register  (read/write)
    };

    enum Register_value
    {
      IIR_BUSY = 7,
    };

  public:
    enum
      {
        PAR_NONE = 0x00,
        PAR_EVEN = 0x18,
        PAR_ODD  = 0x08,
        DAT_5    = 0x00,
        DAT_6    = 0x01,
        DAT_7    = 0x02,
        DAT_8    = 0x03,
        STOP_1   = 0x00,
        STOP_2   = 0x04,

        MODE_8N1 = PAR_NONE | DAT_8 | STOP_1,
        MODE_7E1 = PAR_EVEN | DAT_7 | STOP_1,

        // these two values are to leave either mode
        // or baud rate unchanged on a call to change_mode
        MODE_NC  = 0x1000000,
        BAUD_NC  = 0x1000000,

        Base_rate_x86 = 115200,
        Base_rate_pxa = 921600,
      };

    enum Init_flags
    {
      F_skip_init = 1,
    };

    explicit Uart_16550(unsigned long base_rate, unsigned char init_flags = 0,
                        unsigned char ier_bits = 0,
                        unsigned char mcr_bits = 0, unsigned char fcr_bits = 0)
    : _base_rate(base_rate), _init_flags(init_flags), _mcr_bits(mcr_bits),
      _ier_bits(ier_bits), _fcr_bits(fcr_bits)
    {}

    bool startup(Io_register_block const *regs);
    void shutdown();
    bool change_mode(Transfer_mode m, Baud_rate r);
    int get_char(bool blocking = true) const;
    int char_avail() const;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count) const;
    bool enable_rx_irq(bool enable = true);

  private:
    unsigned long _base_rate;
    unsigned char _init_flags;
    unsigned char _mcr_bits;
    unsigned char _ier_bits;
    unsigned char _fcr_bits;
  };
}
