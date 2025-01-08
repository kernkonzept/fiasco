#pragma once

#include "uart_base.h"
#include "of1275.h"

namespace L4
{
  class Uart_mpc52xx : public Uart
  {
  private:
    typedef unsigned char  u8;
    typedef unsigned short u16;
    typedef unsigned int   u32;

    /**
     * PSC registers
     */
    //psc + 0x00, size u8
    unsigned long mode() const
    { return 0; }

    //psc + 0x04, size u16
    unsigned long status() const
    { return 0x04; }

    //psc + 0x08, size u8
    unsigned long command() const
    { return 0x08; }

    //psc + 0x0c, size u32
    unsigned long buffer() const
    { return 0x0c; }

    //psc + 0x14, size u16
    unsigned long imr() const
    { return 0x14; }

    unsigned long sicr() const
    { return 0x44; }

    template < typename T >
    void wr(unsigned long addr, T val) const;

    template < typename T >
    T rd(unsigned long addr) const;

    template < typename T >
    void wr_dirty(unsigned long addr, T val) const;

    template < typename T >
    T rd_dirty(unsigned long addr) const;

    inline void mpc52xx_out_char(char c) const;

  public:
    explicit Uart_mpc52xx(unsigned /*base_rate*/) {}
    bool startup(const L4::Io_register_block *) override;
    void shutdown() override;
    bool enable_rx_irq(bool enable = true) override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count, bool blocking = true)
      const override;
  };
};

#endif
