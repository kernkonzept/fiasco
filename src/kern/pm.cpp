INTERFACE:

#include <cxx/dlist>
#include "per_cpu_data.h"

#include <cassert>

class Pm_object : public cxx::D_list_item
{
private:
  typedef cxx::Sd_list<Pm_object, cxx::D_list_item_policy, cxx::Sd_list_head_policy<Pm_object>, true> List;
public:
  virtual void pm_on_suspend(Cpu_number) = 0;
  virtual void pm_on_resume(Cpu_number) = 0;
  virtual ~Pm_object() = 0;

  void register_pm(Cpu_number cpu)
  {
    // A Pm_object can only be enqueued into one list as it is
    // a list element itself.
    assert(cxx::D_list_item_policy::next(this) == 0);
    _list.cpu(cpu).push_back(this);
  }

  static void run_on_suspend_hooks(Cpu_number cpu)
  {
    List &l = _list.cpu(cpu);
    for (List::R_iterator c = l.rbegin(); c != l.rend(); ++c)
      (*c)->pm_on_suspend(cpu);
  }

  static void run_on_resume_hooks(Cpu_number cpu)
  {
    List &l = _list.cpu(cpu);
    for (auto const &&c: l)
      c->pm_on_resume(cpu);
  }

private:
  static Per_cpu<List> _list;
};


IMPLEMENTATION:

DEFINE_PER_CPU Per_cpu<Pm_object::List> Pm_object::_list;

IMPLEMENT inline
Pm_object::~Pm_object() {}


