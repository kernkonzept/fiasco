/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "uart_tegra-tcu.h"

namespace L4 {

enum
{
  Shift_num_bytes = 24,
  Doorbell        = 1 << 31,
  Busy            = 1 << 31,
};

bool Uart_tegra_tcu::startup(Io_register_block const *regs)
{
  _regs = regs;
  return true;
}

void Uart_tegra_tcu::set_rx_mbox(Io_register_block const *rx_mbox)
{
  _rx_mbox = rx_mbox;
}

void Uart_tegra_tcu::shutdown()
{}

bool Uart_tegra_tcu::change_mode(Transfer_mode, Baud_rate)
{ return true; }

int Uart_tegra_tcu::tx_avail() const
{
  return !(_regs->read<unsigned>(0) & Busy);
}

void Uart_tegra_tcu::out_char(char c) const
{
  _regs->write<unsigned>(0, Doorbell | (1U << Shift_num_bytes) | c);
}

int Uart_tegra_tcu::write(char const *s, unsigned long count, bool blocking) const
{
  return generic_write<Uart_tegra_tcu>(s, count, blocking);
}

int Uart_tegra_tcu::char_avail() const
{
  if (_current_rx_val)
    return true;

  if (_rx_mbox)
    {
      unsigned v = _rx_mbox->read<unsigned>(0);
      return v >> Shift_num_bytes; // Not sure all of v would be 0
    }

  return false;
}

int Uart_tegra_tcu::get_char(bool blocking) const
{
  while (!char_avail())
   if (!blocking)
     return -1;

  if (_current_rx_val == 0)
    {
      if (_rx_mbox)
        {
          unsigned v = _rx_mbox->read<unsigned>(0);
          if (v >> Shift_num_bytes) // Not sure all of v would be 0
            _current_rx_val = v;
        }
    }

  if (_current_rx_val)
    {
      unsigned char ch = _current_rx_val & 0xff;
      unsigned num = (_current_rx_val >> Shift_num_bytes) & 3;

      // rewrite _current_rx_val according to mbox format
      if (num > 1)
        _current_rx_val =   (_current_rx_val & Doorbell)
                          | ((num - 1) << Shift_num_bytes)
                          | ((_current_rx_val & 0xffffff) >> 8);
      else
        {
          _current_rx_val = 0;
          _rx_mbox->write<unsigned>(0, 0);
        }

      return ch;
    }

  return -1;
}

} // namespace L4
