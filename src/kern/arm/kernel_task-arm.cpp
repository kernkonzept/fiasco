IMPLEMENTATION[arm && !hyp]:

#include "config.h"
#include "globals.h"
#include "kmem_space.h"

PRIVATE inline NEEDS["globals.h", "kmem_space.h"]
Kernel_task::Kernel_task()
: Task(Ram_quota::root, Kmem_space::kdir(), Caps::none())
{}

PUBLIC static inline
void
Kernel_task::map_syscall_page(void *p)
{
  auto pte = Kmem_space::kdir()->walk(Virt_addr(Kmem_space::Syscalls),
      Pdir::Depth, true,
      Kmem_alloc::q_allocator(Ram_quota::root));

  if (pte.level == 0) // allocation of second level faild
    panic("FATAL: Error mapping syscall page to %p\n",
          (void *)Kmem_space::Syscalls);

  pte.create_page(Phys_mem_addr(Kmem_space::kdir()->virt_to_phys((Address)p)),
                  Page::Attr(Page::Rights::URX(), Page::Type::Normal(), Page::Kern::Global()));
  pte.write_back_if(true, Mem_unit::Asid_kernel);
}

IMPLEMENTATION[arm && hyp]:

#include "config.h"
#include "globals.h"
#include "kmem_space.h"

PRIVATE inline NEEDS["globals.h", "kmem_space.h"]
Kernel_task::Kernel_task()
: Task(Ram_quota::root, reinterpret_cast<Pdir*>(Kmem_space::kdir()), Caps::none())
{}

PUBLIC static inline
void
Kernel_task::map_syscall_page(void *p)
{
  Mem_space::set_syscall_page(p);
}


