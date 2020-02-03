/*
 * (c) 2008-2012 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *               Alexander Warg <alexander.warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __L4_CXX_UART_SA1000_H__
#define __L4_CXX_UART_SA1000_H__

#include "uart_base.h"

namespace L4
{
  class Uart_sa1000 : public Uart
  {
  public:
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count) const override;
  };
};

#endif
