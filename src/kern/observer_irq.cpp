INTERFACE:

#include "global_data.h"
#include "irq_chip.h"

/**
 * Software triggered interrupt source that allows a single Irq object to be
 * registered as an observer to receive notifications.
 */
class Observer_irq
{
public:
  /**
   * Trigger the registered IRQ object.
   *
   * Does nothing if none is registered.
   *
   * \pre CPU lock must be held.
   *
   * \note This is a potential preemption point, as it might switch to the
   *       receiver of the triggered IRQ!
   */
  void trigger_irq();

  /**
   * Register IRQ object at this observer.
   *
   * \param irq  The IRQ object to register.
   *
   * \retval true   IRQ object registration successful.
   * \retval false  IRQ object registration failed, because a different IRQ
   *                object is already registered at the observer.
   */
  bool register_irq(Irq_base *irq);

  /**
   * Unregister IRQ object from this observer.
   *
   * Does nothing if none is registered.
   */
  void unregister_irq();

private:
  /**
   * Fake IRQ Chip class for notifications. This chip uses the IRQ number as a
   * pointer to the bound Observer_irq object and implements the bind and unbind
   * functionality.
   */
  class Observer_irq_chip : public Irq_chip_soft
  {
  public:
    void detach(Irq_base *irq) override;

    static Global_data<Observer_irq_chip> chip;
  };

  Irq_base *_observer = nullptr;
};

// ---------------------------------------------------------------------------
IMPLEMENTATION:

DEFINE_GLOBAL Global_data<Observer_irq::Observer_irq_chip> Observer_irq::Observer_irq_chip::chip;

IMPLEMENT
void
Observer_irq::Observer_irq_chip::detach(Irq_base *irq) override
{
  static_assert(sizeof(decltype(_observer->pin())) == sizeof(Observer_irq *),
                "Unexpected IRQ pin size.");

  reinterpret_cast<Observer_irq*>(irq->pin())->remove_irq(irq);
  Irq_chip_soft::detach(irq);
}

IMPLEMENT inline
void
Observer_irq::trigger_irq()
{
  if (Irq_base *observer = access_once(&_observer))
    observer->hit(nullptr);
}

IMPLEMENT
bool
Observer_irq::register_irq(Irq_base *irq)
{
  if (_observer)
    return false;

  auto g = lock_guard(irq->irq_lock());
  irq->detach();
  Observer_irq_chip::chip->bind(irq, reinterpret_cast<Mword>(this));
  if (cas<Irq_base *>(&_observer, nullptr, irq))
    return true;

  irq->detach();
  return false;
}

IMPLEMENT
void
Observer_irq::unregister_irq()
{
  auto g = lock_guard(cpu_lock);
  if (Irq_base *observer = access_once(&_observer))
    {
      auto g = lock_guard(observer->irq_lock());
      observer->detach();
    }
}

PRIVATE inline
void
Observer_irq::remove_irq(Irq_base *irq)
{
  cas<Irq_base *>(&_observer, irq, nullptr);
}
