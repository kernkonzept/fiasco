/*
 * Copyright (C) 2009-2012 Technische Universität Dresden.
 * Copyright (C) 2023-2025 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <stddef.h>
#include "io_regblock.h"

#include "device.h"
#include "poll_timeout_counter.h"

namespace L4 {

/**
 * Uart driver abstraction.
 */
class Uart : public Dev_obj
{
protected:
  unsigned _mode;
  unsigned _rate;
  Io_register_block const *_regs;

public:
  void *operator new (size_t, void *p) { return p; }

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

#ifndef UART_WITHOUT_INPUT

  /**
   * Enable the receive IRQ.
   *
   * \retval true     The RX IRQ was successfully enabled / disabled.
   * \retval false    The RX IRQ couldn't be enabled / disabled. The
   *                  driver does not support this operation.
   */
  virtual bool enable_rx_irq(bool = true) { return false; }

  /**
   * Acknowledge a received interrupt.
   */
  virtual void irq_ack() {}

  /**
   * Check if there is at least one character available for reading from the
   * UART.
   *
   * \return 0 if there is no character available for reading, !=0 otherwise.
   */
  virtual int char_avail() const = 0;

  /**
   * Read a character from the UART.
   *
   * \param blocking  If true, wait until a character is available for
   *                  reading. Otherwise do not wait and just return -1 if
   *                  no character is available.
   * \return          The actual character read from the UART.
   */
  virtual int get_char(bool blocking = true) const = 0;

#endif // !UART_WITHOUT_INPUT

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

} // namespace L4

/*
 * Instantiate a UART, given the memory.
 */
static inline
L4::Uart *
l4re_dev_uart_create_by_dt_compatible(const char *dt_compatible,
                                      void *obj_buf, unsigned obj_buf_size,
                                      unsigned freq)
{
  L4::Dev_obj *dev = l4re_dev_create_by_dt_compatible(dt_compatible,
                                                      obj_buf, obj_buf_size,
                                                      freq);
  return static_cast<L4::Uart *>(dev);
}

/*
 * Instantiate a UART a single time, use an internal static buffer.
 */
static inline
L4::Uart *
l4re_dev_uart_create_by_dt_compatible_once(const char *dt_compatible, unsigned freq)
{
  static bool once_done = false;

  if (once_done)
    return nullptr;

  once_done = true;
  static char __attribute__((aligned(sizeof(long) * 2))) obj_buf[64];
  return l4re_dev_uart_create_by_dt_compatible(dt_compatible,
                                               obj_buf, sizeof(obj_buf),
                                               freq);
}

#define l4re_register_device_uart(Device_class, instance_name, \
                                  device_dt_ids, pci_ids) \
  static const struct l4re_device_ids __dev_spec_##instance_name \
    = { .dt = device_dt_ids, .pcidev = pci_ids }; \
  l4re_register_device(L4::Uart, Device_class, instance_name, \
                       __dev_spec_##instance_name)

#define l4re_register_device_uart_dt(Device_class, instance_name, dt_ids) \
  l4re_register_device_uart(Device_class, instance_name, dt_ids, nullptr)

#define l4re_register_device_uart_pci(Device_class, instance_name, pci_ids) \
  l4re_register_device_uart(Device_class, instance_name, nullptr, pci_ids)

