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
 * Entry point for a device IRQ IDT vector.
 *
 * IA32 IRQ Chips use arrays of this entry code pieces
 * and dynamically assign adresses of Irq_base objects to them
 * to directly pass device IRQs to an Irq_base object.
 *
 * The chips also use these objects to manage the assignment of Irq_base
 * objects to the pins of the IRQ controller.
 */
class Irq_entry_code : public Boot_alloced
{
private:
  struct
  {
    unsigned char push;
    unsigned char mov;
    Signed32 irq_adr;
    unsigned char jmp;
    Unsigned32 jmp_adr;
    unsigned char vector;
  } __attribute__((packed)) _d;

public:
  Irq_entry_code() { free(); _d.vector = 0; }

  Irq_base *irq() const
  { return (Irq_base*)((Smword)(_d.irq_adr)); }

  bool is_free() const
  { return !_d.push; }

  void free()
  {
    _d.push = 0;
    _d.irq_adr = 0;
  }

  unsigned char vector() const
  { return _d.vector; }
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
  Irq_entry_code *_entry;
  Spin_lock<> _entry_lock;

  static Int_vector_allocator _vectors;
};


IMPLEMENTATION [32bit]:
enum { Register_arg0 = 0 }; // eax

IMPLEMENTATION [64bit]:
enum { Register_arg0 = 7 }; // rdi

IMPLEMENTATION:

#include <cassert>

#include "cpu_lock.h"
#include "idt.h"
#include "mem.h"

// The global INT vector allocator for IRQs uses these data
Int_vector_allocator Irq_chip_ia32::_vectors;

PUBLIC
void
Irq_entry_code::setup(Irq_base *irq = 0, unsigned char vector = 0)
{
  extern char __generic_irq_entry[];
  // push %eax/%rdi
  _d.push = 0x50 + Register_arg0;

  // mov imm32, %eax/%rdi
  _d.mov = 0xb8 + Register_arg0;
  _d.irq_adr = (Address)irq;

  // jmp __generic_irq_entry
  _d.jmp = 0xe9;
  _d.jmp_adr = (Address)__generic_irq_entry - (Address)&_d - 11;


  // no code, our currently assigned IRQ vector
  // 0 means no vector allocated
  _d.vector = vector;
}


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
  _entry(irqs ? new Irq_entry_code[irqs] : 0),
  _entry_lock(Spin_lock<>::Unlocked)
{
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

  return _entry[irqn].irq();
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

  Irq_entry_code *const e = &_entry[pin];

  if (!e->is_free())
    return 0;

  if (!vector)
    vector = e->vector();

  if (!vector)
    vector = _vectors.alloc();

  return vector;
}

PRIVATE
unsigned
Irq_chip_ia32::_vsetup(Irq_base *irq, Mword pin, unsigned vector)
{
  Irq_entry_code *const e = &_entry[pin];
  e->setup(irq, vector);

  // force code to memory before setting IDT entry
  Mem::barrier();

  Idt::set_entry(vector, (Address)e, false);
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
  Irq_entry_code *e = &_entry[irq->pin()];

  assert (!e->is_free());
  assert (e->irq() == irq);

  Idt::set_entry(e->vector(), (Address)handler, false);

  e->free();

  return true;
}


PUBLIC
bool
Irq_chip_ia32::reserve(Mword irqn)
{
  if (irqn >= _irqs)
    return false;

  if (!_entry[irqn].is_free())
    return false;

  _entry[irqn].setup();
  return true;
}
