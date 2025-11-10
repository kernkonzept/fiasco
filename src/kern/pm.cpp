INTERFACE:

#include <cxx/dlist>
#include <cassert>
#include "per_cpu_data.h"

/**
 * CPU-local power management callbacks.
 *
 * Inheriting from this class allows to define callbacks that are executed
 * before suspend and after resume on a CPU specified by calling the
 * #register_pm() method.
 */
class Cpu_pm_callbacks : public cxx::D_list_item
{
private:
  typedef cxx::Sd_list<Cpu_pm_callbacks, cxx::D_list_item_policy,
                       cxx::Sd_list_head_policy<Cpu_pm_callbacks>, true> List;

public:
  /**
   * Suspend callback, to be defined by the inheriting class.
   *
   * \note The callback might be called from a non-thread context and the
   *       implementation should be prepared for that. For example, the
   *       #current_cpu() function cannot be safely called and the provided
   *       argument should be used instead.
   *
   * \param current_cpu  Current CPU number.
   */
  virtual void pm_on_suspend(Cpu_number current_cpu) = 0;

  /**
   * Resume callback, to be defined by the inheriting class.
   *
   * \note The callback might be called from a non-thread context and the
   *       implementation should be prepared for that. For example, the
   *       #current_cpu() function cannot be safely called and the provided
   *       argument should be used instead.
   *
   * \param current_cpu  Current CPU number.
   */
  virtual void pm_on_resume(Cpu_number current_cpu) = 0;

  virtual ~Cpu_pm_callbacks() = 0;

  /**
   * Register power management classbacks on a specific CPU.
   *
   * This method is allowed to be called at most once (since each object
   * inheriting from #Cpu_pm_callbacks can only be enququed into a single
   * per-CPU list of callbacks as it a list element itself).
   *
   * \param cpu  CPU where to run the power management callbacks on.
   */
  void register_pm(Cpu_number cpu)
  {
    assert(cxx::D_list_item_policy::next(this) == nullptr);
    _list.cpu(cpu).push_back(this);
  }

  /**
   * Run power management suspend callbacks registered for the current CPU.
   *
   * \note The CPU number of the current CPU must be explicitly supplied by
   *       the caller. There is no generic way the method can check that the
   *       supplied CPU number is the current CPU since this method might be
   *       called from a non-thread context.
   *
   * \param current_cpu  Current CPU number.
   */
  static void run_on_suspend_hooks(Cpu_number current_cpu)
  {
    List &list = _list.cpu(current_cpu);

    for (List::R_iterator i = list.rbegin(); i != list.rend(); ++i)
      (*i)->pm_on_suspend(current_cpu);
  }

  /**
   * Run power management resume callbacks registered for the current CPU.
   *
   * \note The CPU number of the current CPU must be explicitly supplied by
   *       the caller. There is no generic way the method can check that the
   *       supplied CPU number is the current CPU since this method might be
   *       called from a non-thread context.
   *
   * \param current_cpu  Current CPU number.
   */
  static void run_on_resume_hooks(Cpu_number current_cpu)
  {
    List &list = _list.cpu(current_cpu);

    for (auto const &&i: list)
      i->pm_on_resume(current_cpu);
  }

private:
  static Per_cpu<List> _list;
};

IMPLEMENTATION:

DEFINE_PER_CPU Per_cpu<Cpu_pm_callbacks::List> Cpu_pm_callbacks::_list;

IMPLEMENT inline
Cpu_pm_callbacks::~Cpu_pm_callbacks() {}
