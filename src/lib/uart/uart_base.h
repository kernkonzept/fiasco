/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 */
/*
 * (c) 2009-2012 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <stddef.h>
#include "io_regblock.h"

#include "poll_timeout_counter.h"

namespace L4
{
  class Uart
  {
  protected:
    unsigned _mode;
    unsigned _rate;
    Io_register_block const *_regs;

  public:
    void *operator new (size_t, void* a)
    { return a; }

  public:
    typedef unsigned Transfer_mode;
    typedef unsigned Baud_rate;

    Uart()
    : _mode(~0U), _rate(~0U)
    {}

    /**
     * Start the UART driver.
     *
     * \param regs      IO register block of the UART.
     * \retval true     Startup succeeded.
     * \retval false    Startup failed.
     */
    virtual bool startup(Io_register_block const *regs) = 0;

    virtual ~Uart() {}

    /**
     * Terminate the UART driver. This includes disabling of interrupts.
     */
    virtual void shutdown() = 0;

    /**
     * Set certain parameters of the UART.
     *
     * \param m         UART mode. Depends on the hardware.
     * \param r         Baud rate.
     * \retval true     Mode setting succeeded (or was not performed at all).
     * \retval false    Mode setting failed for some reason.
     *
     * \note Some drivers don't perform any mode setting at all and just return
     *       true.
     */
    virtual bool change_mode(Transfer_mode m, Baud_rate r) = 0;

    /**
     * Read a character from the UART.
     *
     * \param blocking  If true, wait until a character is available for
     *                  reading. Otherwise do not wait and just return -1 if
     *                  no character is available.
     * \return          The actual character read from the UART.
     */
    virtual int get_char(bool blocking = true) const = 0;

    /**
     * Check if there is at least one character available for reading from the
     * UART.
     *
     * \return 0 if there is no character available for reading, !=0 otherwise.
     */
    virtual int char_avail() const = 0;

    /**
     * Transmit a number of characters.
     *
     * \param s         Buffer containing the characters.
     * \param count     Number of characters to transmit.
     * \param blocking  If true, wait until there is space in the transmit
     *                  buffer and also wait until every character was
     *                  successful transmitted. Otherwise do not wait.
     * \return          The number of successfully written characters.
     */
    virtual int write(char const *s, unsigned long count,
                      bool blocking = true) const = 0;

    /**
     * Acknowledge a received interrupt.
     */
    virtual void irq_ack() {}

    /**
     * Enable the receive IRQ.
     *
     * \retval true     The RX IRQ was successfully enabled / disabled.
     * \retval false    The RX IRQ couldn't be enabled / disabled. The
     *                  driver does not support this operation.
     */
    virtual bool enable_rx_irq(bool = true) { return false; }

    /**
     * Return the transfer mode.
     *
     * \return The transfer mode.
     */
    Transfer_mode mode() const { return _mode; }

    /**
     * Return the baud rate.
     *
     * \return The baud rate.
     */
    Baud_rate rate() const { return _rate; }

  protected:
    /**
     * Internal function transmitting each character one-after-another and
     * finally waiting that the transmission did actually finish.
     *
     * \param s          Buffer containing the characters.
     * \param count      The number of characters to transmit.
     * \param blocking   If true, wait until there is space in the transmit
     *                   buffer and also wait until every character was
     *                   successful transmitted. Otherwise do not wait.
     * \return           The number of successful written characters.
     */
    template <typename Uart_driver, bool Timeout_guard = true>
    int generic_write(char const *s, unsigned long count,
                      bool blocking = true) const
    {
      auto *self = static_cast<Uart_driver const*>(this);

      unsigned long c;
      for (c = 0; c < count; ++c)
        {
          if (!blocking && !self->tx_avail())
            break;

          if constexpr (Timeout_guard)
            {
              Poll_timeout_counter i(3000000);
              while (i.test(!self->tx_avail()))
                ;
            }
          else
            {
              while (!self->tx_avail())
                ;
            }

          self->out_char(*s++);
        }

      if (blocking)
        self->wait_tx_done();

      return c;
    }
  };
}
