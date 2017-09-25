INTERFACE:

#include "types.h"

//------------------------------------------------------------------------------
IMPLEMENTATION:

#include "mem_layout.h"
#include "kernel_task.h"
#include "mem_space.h"
#include "vmem_alloc.h"

IMPLEMENT static
void
Sys_call_page::init()
{
#if 0
  Mword *sys_calls = (Mword *)Mem_layout::Syscalls_phys;
  for(unsigned i = 0; i < Config::PAGE_SIZE; i += sizeof(Mword))
    *(sys_calls++) = 0x44000002; //sc

  //insert in cache
  Kernel_task::kernel_task()->mem_space()->v_insert(
	Mem_space::Phys_addr(Mem_layout::Syscalls_phys),
	Mem_space::Addr(Mem_layout::Syscalls),
	Mem_space::Size(Config::PAGE_SIZE),
	Mem_space::Page_cacheable | Mem_space::Page_user_accessible
  );

  //insert in htab
  Kernel_task::kernel_task()->mem_space()->try_htab_fault(Mem_layout::Syscalls);
#endif


  Mword *sys_calls = (Mword*)Mem_layout::Syscalls;
  if (!Vmem_alloc::page_alloc(sys_calls,
                              Vmem_alloc::NO_ZERO_FILL, Vmem_alloc::User))
    panic("FIASCO: can't allocate system-call page.\n");
  for (unsigned i = 0; i < Config::PAGE_SIZE; i += sizeof(Mword))
    *(sys_calls++) = 0x44000002; //sc

  Kernel_task::kernel_task()
    ->set_attributes(Virt_addr(Mem_layout::Syscalls),
	             Page::Attr(Page::Rights::URX(), Page::Type::Normal(),
		                Page::Kern::Global()));

  //Mem_unit::flush_cache();
}
