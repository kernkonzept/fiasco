INTERFACE:

#include <cassert>

#include "panic.h"
#include "per_cpu_data.h"
#include "types.h"

class Timeout;

extern Per_cpu<Timeout *> timeslice_timeout;

/* the check macro is like assert(), but it evaluates its argument
   even if NDEBUG is defined */
#ifndef check
#ifdef NDEBUG
# define check(expression) ((void)(expression))
#else /* ! NDEBUG */
# define check(expression) assert(expression)
#endif /* NDEBUG */
#endif /* check */

class Kobject_iface;

class Initial_kobjects
{
public:
  enum Initial_cap
  {
    Task      =  1,
    Factory   =  2,
    Thread    =  3,
    Pager     =  4,
    Log       =  5,
    Icu       =  6,
    Scheduler =  7,
    Iommu     =  8,
    Jdb       = 10,

    First_alloc_cap = Log,
    Num_alloc       = 6,
    End_alloc_cap   = First_alloc_cap + Num_alloc,
  };

  static Cap_index first() { return Cap_index(First_alloc_cap); }
  static Cap_index end() { return Cap_index(End_alloc_cap); }

  void register_obj(Kobject_iface *o, Initial_cap cap)
  {
    assert (cap >= First_alloc_cap);
    assert (cap < End_alloc_cap);

    int c = cap - First_alloc_cap;

    assert (!_v[c]);

    _v[c] = o;
  }

  Kobject_iface *obj(Cap_index cap) const
  {
    assert (cap >= first());
    assert (cap < end());

    return _v[cxx::int_value<Cap_diff>(cap - first())];
  }

private:
  Kobject_iface *_v[Num_alloc];
};


extern Initial_kobjects initial_kobjects;


//---------------------------------------------------------------------------
IMPLEMENTATION:

Initial_kobjects initial_kobjects;


