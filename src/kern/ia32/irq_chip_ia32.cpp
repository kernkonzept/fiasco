INTERFACE[amd64]:

class Irq_base;

/** this structure must exactly map to the code stubs from 64/entry.S */
struct Irq_entry_stub
{
  char _res[3];
  Irq_base *irq;
  char _res2[5];
} __attribute__((packed));

INTERFACE[ia32 || ux]:

class Irq_base;

/** this structure must exactly map to the code stubs from 32/entry.S */
struct Irq_entry_stub
{
  char _res[2];
  Irq_base *irq;
  char _res2[5];
} __attribute__((packed));

INTERFACE:

#include "globals.h"
#include "idt_init.h"
#include "irq_chip.h"
#include "boot_alloc.h"
#include "spin_lock.h"
#include "lock_guard.h"

/**
 * Allocator for IA32 interrupt vectors in the IDT.
 *
 * Some vectors are fixed purpose, others can be dynamically
 * managed by this allocator to support MSIs and multiple IO-APICs.
 */
class Int_vector_allocator
{
public:
  enum
  {
    /// Start at vector 0x20, note: <0x10 is vorbidden here
    Base = 0x20,

    /// The Last vector + 1 that is managed
    End  = APIC_IRQ_BASE - 0x08
  };

  bool empty() const { return !_first; }

private:
  /// array for free list
  unsigned char _vectors[End - Base];

  /// the first free vector
  unsigned _first;

  Spin_lock<> _lock;
};

/**
 * Generic IA32 IRQ chip class.
 *
 * Uses an array of Idt_entry_code objects to manage
 * the IRQ entry points and the Irq_base objects assigned to the
 * pins of a specific controller.
 */
class Irq_chip_ia32
{
public:
  /// Number of pins at this chip.
  unsigned nr_irqs() const { return _irqs; }

protected:
  unsigned _irqs;
  unsigned char *_vec;
  Spin_lock<> _entry_lock;

  static Int_vector_allocator _vectors;

  unsigned char vector(Mword pin) const
  { return _vec[pin]; }
};


IMPLEMENTATION:

#include <cassert>

#include "cpu_lock.h"
#include "idt.h"
#include "mem.h"

// The global INT vector allocator for IRQs uses these data
Int_vector_allocator Irq_chip_ia32::_vectors;


/**
 * Add free vectors to the allocator.
 * \note This code is not thread / MP safe and assumed to be executed at boot
 * time.
 */
PUBLIC
void
Int_vector_allocator::add_free(unsigned start, unsigned end)
{
  assert (Base > 0x10);
  assert (End > Base);
  assert (start >= Base);
  assert (end <= End);

  for (unsigned v = start - Base; v < end - Base - 1; ++v)
    _vectors[v] = v + Base + 1;

  _vectors[end - Base - 1] = _first;
  _first = start;
}

PUBLIC inline
void
Int_vector_allocator::free(unsigned v)
{
  assert (Base <= v && v < End);

  auto g = lock_guard(_lock);
  _vectors[v - Base] = _first;
  _first = v;
}

PUBLIC inline
unsigned
Int_vector_allocator::alloc()
{
  if (!_first)
    return 0;

  auto g = lock_guard(_lock);
  unsigned r = _first;
  if (!r)
    return 0;

  _first = _vectors[r - Base];
  return r;
}

/**
 * \note This code is not thread / MP safe.
 */
PUBLIC explicit inline
Irq_chip_ia32::Irq_chip_ia32(unsigned irqs)
: _irqs(irqs),
  _vec(irqs ? (unsigned char *)Boot_alloced::alloc(irqs) : 0),
  _entry_lock(Spin_lock<>::Unlocked)
{
  for (unsigned i = 0; i < irqs; ++i)
    _vec[i] = 0;

  // add vectors from 0x40 up to Int_vector_allocator::End
  // as free if we are the first IA32 chip ctor running
  if (_vectors.empty())
    _vectors.add_free(0x34, Int_vector_allocator::End);
}


PUBLIC
Irq_base *
Irq_chip_ia32::irq(Mword irqn) const
{
  if (irqn >= _irqs)
    return 0;

  if (!_vec[irqn])
    return 0;

  extern Irq_entry_stub idt_irq_vector_stubs[];
  return idt_irq_vector_stubs[_vec[irqn] - 0x20].irq;
}

/**
 * Generic binding of an Irq_base object to a specific pin and a 
 * an INT vector.
 *
 * \param irq The Irq_base object to bind
 * \param pin The pin number at this IRQ chip
 * \param vector The INT vector to use, or 0 for dynamic allocation
 * \return the INT vector used an success, or 0 on failure.
 *
 * This function does the following:
 * 1. Some sanity checks
 * 2. Check if PIN is unassigned
 * 3. Check if no vector is given:
 *    a) Use vector that was formerly assigned to this PIN
 *    b) Try to allocate a new vector for the PIN
 * 4. Prepare the entry code to point to \a irq
 * 5. Point IDT entry to the PIN's entry code
 * 6. Return the assigned vector number
 */
PRIVATE
unsigned
Irq_chip_ia32::_valloc(Mword pin, unsigned vector)
{
  if (pin >= _irqs)
    return 0;

  if (vector >= Int_vector_allocator::End)
    return 0;

  if (_vec[pin])
    return 0;

  if (!vector)
    vector = _vectors.alloc();

  return vector;
}

PRIVATE
unsigned
Irq_chip_ia32::_vsetup(Irq_base *irq, Mword pin, unsigned vector)
{
  _vec[pin] = vector;
  extern Irq_entry_stub idt_irq_vector_stubs[];
  auto p = idt_irq_vector_stubs + vector - 0x20;
  p->irq = irq;

  // force code to memory before setting IDT entry
  Mem::barrier();

  Idt::set_entry(vector, (Address)p, false);
  return vector;
}

/**
 * \pre `irq->irqLock()` must be held
 */
PROTECTED template<typename CHIP> inline
unsigned
Irq_chip_ia32::valloc(Irq_base *irq, Mword pin, unsigned vector)
{
  auto g = lock_guard(_entry_lock);
  unsigned v = _valloc(pin, vector);
  if (!v)
    return 0;

  static_cast<CHIP*>(this)->bind(irq, pin);
  _vsetup(irq, pin, v);
  return v;
}

/**
 * \pre `irq->irqLock()` must be held
 */
PROTECTED
bool
Irq_chip_ia32::vfree(Irq_base *irq, void *handler)
{
  extern Irq_entry_stub idt_irq_vector_stubs[];
  unsigned v = _vec[irq->pin()];
  assert (v);
  assert (idt_irq_vector_stubs[v - 0x20].irq == irq);

  Idt::set_entry(v, (Address)handler, false);
  idt_irq_vector_stubs[v - 0x20].irq = 0;
  _vec[irq->pin()] = 0;

  _vectors.free(v);
  return true;
}


PUBLIC
bool
Irq_chip_ia32::reserve(Mword irqn)
{
  if (irqn >= _irqs)
    return false;

  if (_vec[irqn])
    return false;

  _vec[irqn] = 0xff;
  return true;
}
