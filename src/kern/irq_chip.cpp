INTERFACE:

#include <cxx/bitfield>
#include "atomic.h"
#include "types.h"
#include "spin_lock.h"
#include "global_data.h"

class Irq_base;
class Irq_chip;

INTERFACE [cascade_irq]:

/**
 * When cascading IRQ controllers, we need to ack upstream IRQs before
 * activating the handler thread and after possibly masking the IRQ at the
 * downstream controller. To do this we use a linked list of upstream IRQ
 * objects on the callstack of the handler to describe the upstream IRQs to ack.
 */
class Upstream_irq
{
private:
  Irq_chip *const _c;
  Mword const _p;
  Upstream_irq const *const _prev;
};

INTERFACE [!cascade_irq]:

/**
 * The Upstream_irq mechanism is only required and used for cascading IRQ
 * controllers. So in case that feature is disabled, reduce Upstream_irq to a
 * immediately apparent nop, which is only a placeholder for generic IRQ
 * handling code.
 */
class Upstream_irq
{
public:
  // Upstream_irq cannot be instantiated.
  Upstream_irq() = delete;
  static void ack(Upstream_irq const *) {}
};

// --------------------------------------------------------------------------
INTERFACE:

/**
 * Abstraction for an IRQ controller chip.
 */
class Irq_chip
{
public:
  struct Mode
  {
    Mode() = default;
    explicit Mode(Mword m) : raw(m) {}

    enum Flow_type
    {
      Set_irq_mode  = 0x1,
      Trigger_edge  = 0x0,
      Trigger_level = 0x2,
      Trigger_mask  = 0x2,

      Polarity_high = 0x0,
      Polarity_low  = 0x4,
      Polarity_both = 0x8,
      Polarity_mask = 0xc,

      F_level_high   = Set_irq_mode | Trigger_level | Polarity_high,
      F_level_low    = Set_irq_mode | Trigger_level | Polarity_low,
      F_raising_edge = Set_irq_mode | Trigger_edge  | Polarity_high,
      F_falling_edge = Set_irq_mode | Trigger_edge  | Polarity_low,
      F_both_edges   = Set_irq_mode | Trigger_edge  | Polarity_both
    };

    Mode(Flow_type ft) : raw(ft) {}

    Mword raw;
    CXX_BITFIELD_MEMBER(0, 0, set_mode, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(1, 3, flow_type, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(1, 1, level_triggered, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(2, 3, polarity, raw);
    CXX_BITFIELD_MEMBER(4, 5, wakeup, raw);
    CXX_BITFIELD_MEMBER(4, 4, set_wakeup, raw);
    CXX_BITFIELD_MEMBER(5, 5, clear_wakeup, raw);
  };

  virtual void mask(Mword pin) = 0;
  virtual void mask_percpu(Cpu_number, Mword pin)
  { mask(pin); }
  virtual void unmask(Mword pin) = 0;
  virtual void unmask_percpu(Cpu_number, Mword pin)
  { unmask(pin); }
  virtual void ack(Mword pin) = 0;
  virtual void mask_and_ack(Mword pin) = 0;

  /**
   * Set the trigger mode and polarity.
   */
  virtual int set_mode(Mword pin, Mode) = 0;
  virtual int set_mode_percpu(Cpu_number, Mword pin, Mode m)
  { return set_mode(pin, m); }
  virtual bool is_edge_triggered(Mword pin) const = 0;

  /**
   * Set the target CPU of an interrupt pin.
   *
   * \param pin  Interrupt pin to configure.
   * \param cpu  Target logical CPU.
   *
   * \retval true   Target CPU set successfully.
   * \retval false  Target CPU not set (failure or unsupported).
   */
  virtual bool set_cpu(Mword pin, Cpu_number cpu) = 0;

  virtual void detach(Irq_base *irq);
  virtual ~Irq_chip() = 0;
};

/**
 * Artificial IRQ chip, used for SW IRQs.
 */
class Irq_chip_soft : public Irq_chip
{
public:
  void mask(Mword) override {}
  void unmask(Mword) override {}
  void mask_and_ack(Mword) override {}
  void ack(Mword) override {}

  int set_mode(Mword, Mode) override { return 0; }
  bool is_edge_triggered(Mword) const override { return true; }
  bool set_cpu(Mword, Cpu_number) override { return false; }

  static Global_data<Irq_chip_soft> sw_chip;
};

/**
 * Abstract IRQ controller chip that is visible as part of the
 * Icu to the user.
 */
class Irq_chip_icu : public Irq_chip
{
public:
  /** Reserve the given pin of the ICU making it impossible to attach an IRQ. */
  virtual bool reserve(Mword pin) = 0;

  /**
   * Attach an IRQ object to a given pin of the ICU.
   *
   * \param irq   The IRQ.
   * \param pin   The pin to attach the IRQ to.
   * \param init  By default, perform the pin allocation and the following
   *              initialization: Adjust the trigger mode of the IRQ to the pin
   *              of the chip, mask/unmask the pin according to the IRQ`s masked
   *              state and set the target CPU of the pin.
   *              If this parameter is set to `false`, skip the initialization.
   */
  virtual bool attach(Irq_base *irq, Mword pin, bool init = true) = 0;

  /** Return the IRQ object attached to a given pin of the ICU. */
  virtual Irq_base *irq(Mword pin) const = 0;

  /** Return the number of pins provided by this ICU. */
  virtual unsigned nr_pins() const = 0;
  virtual ~Irq_chip_icu() = 0;
};

class Kobject_iface;

/**
 * Base class for all kinds of IRQ consuming objects.
 */
class Irq_base
{
  friend class Irq_chip;

public:
  typedef void (*Hit_func)(Irq_base *, Upstream_irq const *);
  using Ptr = Irq_base *;

  enum Flags : Mword
  {
    F_enabled = 1, // This flags needs to be set atomically.
  };

  Irq_base() : _flags(0)
  {
    Irq_chip_soft::sw_chip->bind(this, 0, true);
    mask();
  }

  void hit(Upstream_irq const *ui) { hit_func(this, ui); }

  Mword pin() const { return _pin; }
  Irq_chip *chip() const { return _chip; }
  Spin_lock<> *irq_lock() { return &_irq_lock; }

  void mask()
  {
    if (!__mask())
      _chip->mask(_pin);
  }

  void mask_and_ack()
  {
    if (!__mask())
      _chip->mask_and_ack(_pin);
    else
      _chip->ack(_pin);
  }

  void unmask()
  {
    if (__unmask())
      _chip->unmask(_pin);
  }

  void ack() { _chip->ack(_pin); }

  bool set_cpu(Cpu_number cpu) { return _chip->set_cpu(_pin, cpu); }

  void detach() { _chip->detach(this); }

  bool masked() const { return !(access_once(&_flags) & F_enabled); }
  Mword flags() const { return access_once(&_flags); }

  /**
   * Set the local IRQ state to `masked` (clear the `F_enabled` bit).
   * \retval false the IRQ was not masked before and is now masked.
   * \retval true the IRQ was masked before and the state was NOT changed.
   */
  bool __mask()
  {
    Mword old;
    // Replace by atomic_fetch_and()!
    do
      {
        old = _flags;
        if (!(old & F_enabled))
          return true;
      }
    while (!cas(&_flags, old, old & ~F_enabled));

    return false;
  }

  /**
   * Set the local state to `unmasked` (set the `F_enabled` bit).
   * \retval false the IRQ was not masked before and the state was NOT changed.
   * \retval true the IRQ was masked before and is now unmasked.
   */
  bool __unmask()
  {
    Mword old;
    // Replace by atomic_fetch_or()!
    do
      {
        old = _flags;
        if (old & F_enabled)
          return false;
      }
    while (!cas(&_flags, old, old | F_enabled));

    return true;
  }

  void set_hit(Hit_func f) { hit_func = f; }
  virtual void switch_mode(bool is_edge_triggered) = 0;
  virtual ~Irq_base() = 0;

protected:
  Hit_func hit_func;

  Irq_chip *_chip;
  Mword _pin;
  Mword _flags;
  Spin_lock<> _irq_lock;

  template<typename T>
  static void FIASCO_FLATTEN
  handler_wrapper(Irq_base *irq, Upstream_irq const *ui)
  { nonull_static_cast<T *>(irq)->handle(ui); }

public:
  static Global_data<Irq_base *(*)(Kobject_iface *)> dcast;
};

//----------------------------------------------------------------------------
INTERFACE [debug]:

EXTENSION class Irq_chip
{
public:
  virtual char const *chip_type() const = 0;
};

//--------------------------------------------------------------------------
IMPLEMENTATION[debug]:

PUBLIC
char const *
Irq_chip_soft::chip_type() const override
{ return "Soft"; }

//--------------------------------------------------------------------------
IMPLEMENTATION [cascade_irq]:

PUBLIC inline explicit
Upstream_irq::Upstream_irq(Irq_base const *b, Upstream_irq const *prev)
: _c(b->chip()), _p(b->pin()), _prev(prev)
{}

PUBLIC static inline
void
Upstream_irq::ack(Upstream_irq const *ui)
{
  for (Upstream_irq const *c = ui; c; c = c->_prev)
    c->_c->ack(c->_p);
}

//--------------------------------------------------------------------------
IMPLEMENTATION:

#include "types.h"
#include "cpu_lock.h"
#include "lock_guard.h"
#include "static_init.h"

DEFINE_GLOBAL_PRIO(EARLY_INIT_PRIO)
Global_data<Irq_chip_soft> Irq_chip_soft::sw_chip;

DEFINE_GLOBAL Global_data<Irq_base *(*)(Kobject_iface *)> Irq_base::dcast;

IMPLEMENT inline Irq_chip::~Irq_chip() {}
IMPLEMENT inline Irq_chip_icu::~Irq_chip_icu() {}
IMPLEMENT inline Irq_base::~Irq_base() {}

/**
 * Bind the passed IRQ object to this IRQ controller chip.
 *
 * \param irq        The IRQ object to bind.
 * \param pin        The pin of this IRQ controller chip.
 * \param ctor_only  On `false` (default), set the IRQ mode (edge/level) as
 *                   implemented by the IRQ controller chip and mask/unmask the
 *                   IRQ at the chip according to the current state of the IRQ
 *                   object.
 *                   On `true`, skip these efforts.
 *
 * \pre `irq->irq_lock()` must be held.
 */
PUBLIC inline
void
Irq_chip::bind(Irq_base *irq, Mword pin, bool ctor_only = false)
{
  irq->_pin = pin;
  irq->_chip = this;

  if (ctor_only)
    return;

  irq->switch_mode(is_edge_triggered(pin));

  if (irq->masked())
    mask(pin);
  else
    unmask(pin);
}

/**
 * Detach the passed IRQ object from this IRQ controller chip by binding the
 * IRQ object to the `sw_chip` chip.
 *
 * \param irq  The IRQ object to detach.
 *
 * \see Irq_base::Irq_base()
 *
 * \pre `irq->irq_lock()` must be held.
 */
IMPLEMENT inline
void
Irq_chip::detach(Irq_base *irq)
{
  Irq_chip_soft::sw_chip->bind(irq, 0, true);
}

/**
 * \param CHIP must be the dynamic type of the object.
 */
PUBLIC inline
template<typename CHIP>
void
Irq_chip::handle_irq(Mword pin, Upstream_irq const *ui)
{
  // call the irq function of the chip avoiding the
  // virtual function call overhead.
  Irq_base *irq = nonull_static_cast<CHIP *>(this)->CHIP::irq(pin);
  irq->log();
  irq->hit(ui);
}

PUBLIC inline
template<typename CHIP>
void
Irq_chip::handle_multi_pending(Upstream_irq const *ui)
{
  while (Mword pend = nonull_static_cast<CHIP *>(this)->CHIP::pending())
    {
      for (unsigned i = 0; i < sizeof(Mword) * 8; ++i, pend >>= 1)
        {
          if (pend & 1)
            {
              handle_irq<CHIP>(i, ui);
              break; // Read the pending ints again
            }
        }
    }
}

PUBLIC inline NEEDS["lock_guard.h", "cpu_lock.h"]
void
Irq_base::destroy()
{
  auto g = lock_guard(_irq_lock);
  detach();
}

// --------------------------------------------------------------------------
IMPLEMENTATION [!debug]:

PUBLIC inline void Irq_base::log() {}

//-----------------------------------------------------------------------------
INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Irq_base
{
public:
  struct Irq_log : public Tb_entry
  {
    Irq_base *obj;
    Irq_chip *chip;
    Mword pin;
    void print(String_buffer *buf) const;
  };

  static_assert(sizeof(Irq_log) <= Tb_entry::Tb_entry_size);
};

// --------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include <cstdio>
#include "logdefs.h"
#include "kobject_dbg.h"
#include "string_buffer.h"

IMPLEMENT
void
Irq_base::Irq_log::print(String_buffer *buf) const
{
  Kobject_dbg::Const_iterator irq = Kobject_dbg::pointer_to_obj(obj);

  buf->printf("0x%lx/%lu @ chip %s (%p) ", pin, pin, chip->chip_type(),
              static_cast<void *>(chip));

  if (irq != Kobject_dbg::end())
    buf->printf("D:%lx", irq->dbg_id());
  else
    buf->printf("irq=%p", static_cast<void *>(obj));
}

PUBLIC inline NEEDS["logdefs.h"]
void
Irq_base::log()
{
  Context *c = current();
  LOG_TRACE("IRQ-Object triggers", "irq", c, Irq_log,
      l->obj = this;
      l->chip = chip();
      l->pin = pin();
  );
}
