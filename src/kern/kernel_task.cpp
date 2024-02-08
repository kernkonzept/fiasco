INTERFACE:

#include "task.h"
#include "types.h"
#include "global_data.h"

class Kernel_thread;

class Kernel_task : public Task
{
  friend class Kernel_thread;
  friend class Static_object<Kernel_task>;
private:
  static Global_data<Static_object<Kernel_task>> _t;
};

IMPLEMENTATION[!(arm || ppc32 || sparc)]:

#include "config.h"
#include "globals.h"
#include "kmem.h"

PRIVATE inline NEEDS["globals.h"]
Kernel_task::Kernel_task()
: Task(Ram_quota::root, Kmem::kdir, Caps::none())
{}


IMPLEMENTATION:

DEFINE_GLOBAL Global_data<Static_object<Kernel_task>> Kernel_task::_t;

PUBLIC static Task*
Kernel_task::kernel_task()
{ return _t; }

PUBLIC static inline NEEDS[Kernel_task::Kernel_task]
void
Kernel_task::init()
{ _t.construct(); }
