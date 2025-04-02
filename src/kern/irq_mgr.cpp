INTERFACE:

#include "types.h"
#include "irq_chip.h"
#include "l4_types.h"
#include <cxx/type_traits>
#include "global_data.h"

/**
 * Interface used to manage hardware IRQs on a platform.
 *
 * The main purpose of this interface is to allow an abstract mapping of global
 * system interrupt numbers to a chip and pin number pair. The interface
 * provides also some global information about IRQs.
 *
 * In our terminology, global system interrupt numbers form a system-wide
 * namespace that covers both regular IRQs managed by traditional interrupt
 * controllers and message-signaled interrupts (MSIs).
 */
class Irq_mgr
{
public:
  struct Msi_info
  {
    Unsigned64 addr;
    Unsigned32 data;
  };

  /**
   * Chip and pin tuple.
   */
  struct Chip_pin
  {
    // Allow uninitialized instances.
    enum Init { Bss };

    Chip_pin(Init) {}

    /// Invalid entry.
    Chip_pin() : chip(nullptr) {}

    /// Create a chip and pin tuple.
    Chip_pin(Irq_chip_icu *_chip, Mword _pin) : chip(_chip), pin(_pin) {}

    /// The chip.
    Irq_chip_icu *chip;

    /// The pin number local to \a chip.
    Mword pin;

    Irq_base *irq() const
    { return chip->irq(pin); }
  };

  enum Gsi_mask : Mword
  {
    Msi_bit = 0x80000000U
  };

  /// Map legacy (ISA) pin number to global system interrupt number.
  virtual Mword legacy_override(Mword isa_pin) { return isa_pin; }

  /// Get the chip and pin tuple for the given global system interrupt number.
  virtual Chip_pin chip_pin(Mword gsi) const = 0;

  /**
   * Get the count of available global system interrupt numbers representing
   * regular IRQs.
   *
   * \note This count only includes the regular IRQs managed by traditional
   *       interrupt controllers, not message-signaled interrupts (MSIs).
   *
   * \return Count of available global system interrupt numbers representing
   *         regular IRQs.
   */
  virtual unsigned nr_gsis() const = 0;

  /**
   * Get the count of available global system interrupt numbers representing
   * message-signaled interrupts (MSIs).
   *
   * \note This count only includes the message-signaled interrupts, not the
   *       regular IRQs. The actual global system interrupt numbers that
   *       represent MSIs are distinguished by having the bit 31 set.
   *
   * \return Count of available global system interrupt numbers representing
   *         message-signaled interrupts (MSIs).
   *
   * \retval 0  No MSIs supported.
   */
  virtual unsigned nr_msis() const = 0;

  /**
   * Get the message to use for a given MSI.
   *
   * \pre The global system interrupt number needs to be already attached
   *      before using this function.
   *
   * \param      msi      Global system interrupt representing an MSI.
   * \param      source   Optional platform-specific identifier of the
   *                      interrupt source.
   * \param[out] info     Message signaling information to be used by the
   *                      interrupt source (i.e. message address and message
   *                      payload).
   *
   * \retval 0                Message signaling information returned
   *                          succesfully.
   * \retval -L4_err::ENosys  No MSIs supported.
   * \retval -L4_err::ERange  MSIs supported, but the given global system
   *                          interrupt is not an MSI.
   */
  virtual int msi_info([[maybe_unused]] Mword msi,
                       [[maybe_unused]] Unsigned64 source,
                       [[maybe_unused]] Msi_info *info) const
  { return -L4_err::ENosys; }

  /// The pointer to the single global instance of the actual IRQ manager.
  static Global_data<Irq_mgr *> mgr;

  /// Prevent generation of a real virtual delete function
  virtual ~Irq_mgr() = 0;
};

template<typename CHIP>
class Irq_mgr_single_chip : public Irq_mgr
{
public:
  Irq_mgr_single_chip() {}

  template<typename... A>
  explicit Irq_mgr_single_chip(A&&... args) : c(cxx::forward<A>(args)...) {}

  Chip_pin chip_pin(Mword gsi) const override { return Chip_pin(&c, gsi); }
  unsigned nr_gsis() const override { return c.nr_pins(); }
  unsigned nr_msis() const override { return 0; }

  mutable CHIP c;
};

//--------------------------------------------------------------------------
IMPLEMENTATION:

#include "warn.h"

DEFINE_GLOBAL Global_data<Irq_mgr *> Irq_mgr::mgr;

IMPLEMENT inline Irq_mgr::~Irq_mgr() {}

PUBLIC inline
bool
Irq_mgr::gsi_attach(Irq_base *irq, Mword gsi, bool init = true)
{
  Chip_pin cp = chip_pin(gsi);
  if (!cp.chip)
    return false;

  if (!cp.chip->attach(irq, cp.pin, init))
    return false;

  if (init)
    cp.chip->set_cpu(cp.pin, Cpu_number::boot_cpu());

  return true;
}

PUBLIC inline
bool
Irq_mgr::gsi_reserve(Mword gsi)
{
  Chip_pin cp = chip_pin(gsi);
  if (!cp.chip)
    return false;

  return cp.chip->reserve(cp.pin);
}

PUBLIC inline
Irq_base *
Irq_mgr::gsi_irq(Mword gsi) const
{
  Chip_pin cp = chip_pin(gsi);
  if (!cp.chip)
    return nullptr;

  return cp.chip->irq(cp.pin);
}

PUBLIC inline
void
Irq_mgr::gsi_set_cpu(Mword gsi, Cpu_number cpu) const
{
  Chip_pin cp = chip_pin(gsi);
  if (!cp.chip)
    return;

  cp.chip->set_cpu(cp.pin, cpu);
}
