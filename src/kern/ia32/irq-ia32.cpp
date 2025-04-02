/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Martin Decky <martin.decky@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/**
 * \file
 *
 * IRQ sender overrides.
 */

INTERFACE:

EXTENSION class Irq_sender
{
private:
  enum : unsigned
  {
    /**
     * Number of attempts to skip migration after a failed attempt.
     */
    Default_migration_backoff = 10
  };

  /**
   * If non-zero, then skip the given number of migration attempts.
   */
  unsigned _migration_backoff = 0;
};

//-----------------------------------------------------------------------------
IMPLEMENTATION:

/**
 * Migrate to the given CPU.
 *
 * Set the CPU target of the interrupt pin to which this IRQ sender is attached.
 * If the operation fails, refrain from taking any action for a certain number
 * of migration attempts (to avoid potentially costly overhead of the futile
 * search for an available interrupt vector on some platforms, etc.).
 *
 * \param cpu  Target CPU to migrate to.
 */
IMPLEMENT_OVERRIDE inline
void
Irq_sender::migrate(Cpu_number cpu)
{
  if (_migration_backoff > 0)
    {
      /*
       * Refrain from taking any action after a previous failure.
       */

      _migration_backoff--;
      return;
    }

  if (!Cpu::online(cpu))
    return;

  if (!_chip->set_cpu(pin(), cpu))
    _migration_backoff = Default_migration_backoff;
}
