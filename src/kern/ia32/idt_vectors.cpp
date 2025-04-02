/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Martin Decky <martin.decky@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/**
 * \file
 *
 * Multi-CPU interrupt vector manager.
 */

INTERFACE:

#include "types.h"
#include "minmax.h"
#include "config.h"
#include "spin_lock.h"
#include "irq_chip.h"

/**
 * Multi-CPU interrupt vector manager.
 *
 * This class implements a system-wide interrupt vector manager that allows to
 * attach IRQ objects to interrupt vectors on individual CPU cores in order to
 * maximize the total number of interrupt sources available.
 *
 * The class contains only static methods and members.
 */
class Idt_vectors
{
public:
  static constexpr unsigned Invalid_vector = ~0U;

  enum : unsigned
  {
    /**
     * Highest number of supported CPUs.
     *
     * This is derived from the number of Local APIC IDs. In the x2APIC mode,
     * we might support more CPUs, but that is optional.
     */
    Max_cpus = 256,

    /**
     * Number of supported CPUs.
     */
    Nr_cpus = min(unsigned{Config::Max_num_cpus}, Max_cpus),

    /**
     * Number of supported interrupt sources per CPU.
     */
    Nr_ids = Config::Apic_vector_base - Config::Device_vector_base,

    /**
     * Total number of interrupt sources (pins).
     */
    Nr_pins = Nr_cpus * Nr_ids
  };

  /**
   * CPU and vector tuple.
   */
  struct Cpu_vector
  {
    /// Invalid entry.
    Cpu_vector() : cpu(Cpu_number::nil()) {}

    /// Create a CPU and vector tuple.
    Cpu_vector(Cpu_number _cpu, unsigned _vector) : cpu(_cpu), vector(_vector) {}

    /// The CPU.
    Cpu_number cpu;

    /// The vector.
    unsigned vector;
  };

private:
  static constexpr unsigned Invalid_id = ~0U;

  /**
   * CPU and ID tuple.
   */
  struct Cpu_id
  {
    /// Invalid entry.
    Cpu_id() : cpu(Cpu_number::nil()) {}

    /// Create a CPU and ID tuple.
    Cpu_id(Cpu_number _cpu, unsigned _id) : cpu(_cpu), id(_id) {}

    /// The CPU.
    Cpu_number cpu;

    /// The ID.
    unsigned id;
  };

  /**
   * Attached IRQs of a single CPU.
   */
  struct Cpu_entry
  {
    Irq_base::Ptr ids[Nr_ids] = {};

    /// Number of available slots.
    unsigned free = Nr_ids;
  };

  static Spin_lock<> lock;
  static Cpu_entry cpus[Nr_cpus];
};

IMPLEMENTATION:

#include "cpu.h"
#include "lock_guard.h"

Spin_lock<> Idt_vectors::lock;
Idt_vectors::Cpu_entry Idt_vectors::cpus[Idt_vectors::Nr_cpus];

/**
 * Attach an IRQ to any available vector of any CPU.
 *
 * In the current implementation, the first available vector of the CPU with
 * the most available vectors is chosen for the purpose of static load
 * balancing.
 *
 * \param irq  IRQ to attach.
 *
 * \return CPU and vector tuple to which the IRQ has been attached.
 *
 * \retval Cpu_vector()  No vector is available on any CPU.
 */
PUBLIC static inline
NEEDS[Idt_vectors::find_cpu, Idt_vectors::_attach_cpu, Idt_vectors::id_to_vector]
Idt_vectors::Cpu_vector
Idt_vectors::attach_any(Irq_base *irq)
{
  auto guard = lock_guard(&lock);

  Cpu_number cpu = find_cpu();
  if (cpu == Cpu_number::nil())
    return Cpu_vector();

  unsigned id = _attach_cpu(cpu, irq);
  if (id == Invalid_id)
    return Cpu_vector();

  return Cpu_vector(cpu, id_to_vector(id));
}

/**
 * Attach an IRQ to any available vector of a given CPU.
 *
 * In the current implementation, the first available vector of the given CPU
 * is chosen.
 *
 * \param cpu  CPU to attach to the IRQ to.
 * \param irq  IRQ to attach.
 *
 * \return Vector to which the IRQ has been attached on the given CPU.
 *
 * \retval Invalid_vector  No vector is available on the given CPU.
 */
PUBLIC static inline NEEDS[Idt_vectors::_attach_cpu, Idt_vectors::id_to_vector]
unsigned
Idt_vectors::attach_cpu(Cpu_number cpu, Irq_base *irq)
{
  auto guard = lock_guard(&lock);
  return id_to_vector(_attach_cpu(cpu, irq));
}

/**
 * Move an IRQ attached to the given CPU and vector to a different CPU.
 *
 * Move an IRQ that has been previously attached to a given CPU and vector to
 * a different CPU. In the current implementation, the first available vector
 * of the different CPU is chosen as the new vector.
 *
 * \note The method guarantees that a valid IRQ stays attached, either to the
 *       original CPU (and original vector) or to the different CPU (and
 *       generally a different vector).
 *
 * \param prev_cpu     Original CPU to which an IRQ is attached.
 * \param prev_vector  Original vector to which an IRQ is attached.
 * \param cpu          Different CPU to move the IRQ to.
 *
 * \return Vector to which the IRQ has been attached on the different CPU.
 *
 * \retval Invalid_vector  No vector is available on the different CPU or the
 *                         original CPU, vector pair is invalid (out of range,
 *                         no IRQ attached, etc.).
 */
PUBLIC static inline
NEEDS[Idt_vectors::_attach_cpu, Idt_vectors::_detach, Idt_vectors::vector_to_id,
      Idt_vectors::id_to_vector]
unsigned
Idt_vectors::migrate_vector(Cpu_number prev_cpu, unsigned prev_vector,
                            Cpu_number cpu)
{
  auto guard = lock_guard(&lock);

  if (cxx::int_value<Cpu_number>(prev_cpu) >= Nr_cpus)
    return Invalid_vector;

  if (prev_vector < Config::Device_vector_base)
    return Invalid_vector;

  unsigned prev_id = vector_to_id(prev_vector);
  if (prev_id >= Nr_ids)
    return Invalid_vector;

  Irq_base *irq = cpus[cxx::int_value<Cpu_number>(prev_cpu)].ids[prev_id];
  if (!irq)
    return Invalid_vector;

  unsigned id = _attach_cpu(cpu, irq);
  if (id == Invalid_id)
    return Invalid_vector;

  if (!_detach(prev_cpu, prev_id))
    return Invalid_vector;

  return id_to_vector(id);
}

/**
 * Detach an IRQ attached to the given CPU and vector.
 *
 * If successful, the given vector on the given CPU is made available.
 *
 * \param cpu     CPU to detach the IRQ from.
 * \param vector  Vector to detach the IRQ from.
 *
 * \retval true   The IRQ has been detached and the CPU, vector pair has been
 *                made available.
 * \retval false  The CPU, vector pair is invalid (out of range, no IRQ
 *                attached, etc.)
 */
PUBLIC static inline NEEDS[Idt_vectors::_detach, Idt_vectors::vector_to_id]
bool
Idt_vectors::detach(Cpu_number cpu, unsigned vector)
{
  auto guard = lock_guard(&lock);

  if (cxx::int_value<Cpu_number>(cpu) >= Nr_cpus)
    return false;

  if (vector < Config::Device_vector_base)
    return false;

  unsigned id = vector_to_id(vector);
  if (id >= Nr_ids)
    return false;

  return _detach(cpu, id);
}

/**
 * Get IRQ attached to the given vector on current CPU.
 *
 * \note The method does not perform any locking, thus it can be safely used
 *       from an interrupt context. It is assumed that the update of the actual
 *       IRQ slots is atomic.
 *
 * \param vector  Vector to examine.
 *
 * \return IRQ attached to the given vector on current CPU.
 *
 * \retval nullptr  No attached IRQ (i.e. an available vector) or arguments out
 *                  of bounds.
 */
PUBLIC static inline NEEDS[Idt_vectors::vector_to_id]
Irq_base *
Idt_vectors::get(Mword vector)
{
  if (vector < Config::Device_vector_base)
    return nullptr;

  unsigned id = vector_to_id(vector);
  if (id >= Nr_ids)
    return nullptr;

  unsigned cpu = cxx::int_value<Cpu_number>(current_cpu());
  return cpus[cpu].ids[id];
}

/**
 * Convert per-CPU interrupt source to per-CPU vector.
 *
 * \note No bounds checking is performed.
 *
 * \param id  Per-CPU interrupt source.
 *
 * \return Per-CPU vector.
 */
PRIVATE static inline
unsigned
Idt_vectors::id_to_vector(unsigned id)
{
  return id + Config::Device_vector_base;
}

/**
 * Convert per-CPU vector to per-CPU interrupt source.
 *
 * \note No bounds checking is performed.
 *
 * \param vector  Per-CPU vector
 *
 * \return Per-CPU interrupt source.
 */
PRIVATE static inline
unsigned
Idt_vectors::vector_to_id(unsigned vector)
{
  return vector - Config::Device_vector_base;
}

/**
 * Attach an IRQ to any available interrupt source of a given CPU.
 *
 * In the current implementation, the first available interrupt source of the
 * given CPU is chosen.
 *
 * \note Assumes that `lock` is already held.
 *
 * \param cpu  CPU to attach to the IRQ to.
 * \param irq  IRQ to attach.
 *
 * \return Interrupt source to which the IRQ has been attached on the given
 *         CPU.
 *
 * \retval Invalid_id  No interrupt source is available on the given CPU.
 */
PRIVATE static inline
unsigned
Idt_vectors::_attach_cpu(Cpu_number cpu, Irq_base *irq)
{
  Cpu_entry &entry = cpus[cxx::int_value<Cpu_number>(cpu)];

  for (unsigned id = 0; id < Nr_ids; ++id)
    {
      if (!entry.ids[id])
        {
          entry.ids[id] = irq;
          entry.free--;
          return id;
        }
    }

  return Invalid_id;
}

/**
 * Detach an IRQ attached to the given CPU and interrupt source.
 *
 * If successful, the given interrupt source on the given CPU is made
 * available.
 *
 * \note Assumes that `lock` is already held.
 *
 * \note No bounds checking is performed.
 *
 * \param cpu  CPU to detach the IRQ from.
 * \param id   Interrupt source to detach the IRQ from.
 *
 * \retval true   The IRQ has been detached and the CPU, source pair has been
 *                made available.
 * \retval false  The CPU, source pair has no IRQ attached.
 */
PRIVATE static inline
bool
Idt_vectors::_detach(Cpu_number cpu, unsigned id)
{
  Cpu_entry &entry = cpus[cxx::int_value<Cpu_number>(cpu)];

  if (!entry.ids[id])
    return false;

  entry.ids[id] = nullptr;
  entry.free++;

  return true;
}

/**
 * Find CPU with the most available vectors.
 *
 * \return CPU with the most available vectors.
 *
 * \retval Cpu_number::nil()  No CPU has an available vector.
 */
PRIVATE static inline
Cpu_number
Idt_vectors::find_cpu()
{
  unsigned free = 0;
  Cpu_number cpu = Cpu_number::nil();

  for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    {
      // Ignore all CPUs above Nr_cpus.
      unsigned nr = cxx::int_value<Cpu_number>(i);
      if (nr >= Nr_cpus)
        break;

      // Ignore an offline CPU.
      if (!Cpu::online(i))
        continue;

      // Find the CPU with the most available vectors.
      unsigned i_free = cpus[nr].free;
      if (i_free > free)
        {
          free = i_free;
          cpu = i;

          // We cannot possibly get more than Nr_ids available vectors.
          if (free == Nr_ids)
            break;
        }
    }

  return cpu;
}
