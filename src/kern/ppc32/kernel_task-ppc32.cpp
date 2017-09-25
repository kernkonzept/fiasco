IMPLEMENTATION[ppc32]:

#include "config.h"
#include "globals.h"
#include "space.h"

PRIVATE inline NEEDS["globals.h"]
Kernel_task::Kernel_task()
: Task(Ram_quota::root, Kmem::kdir(), Caps::none())
{}
