IMPLEMENTATION[arm]:

#include "globals.h"
#include "kmem_space.h"

extern char kernel_l0_dir[[]];

PRIVATE
Kernel_task::Kernel_task()
: Task(Ram_quota::root, reinterpret_cast<Pdir*>(&kernel_l0_dir), Caps::none())
{}


