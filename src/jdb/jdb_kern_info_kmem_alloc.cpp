IMPLEMENTATION:

#include "static_init.h"
#include "jdb_kern_info.h"
#include "kmem_alloc.h"
#include "kmem_slab.h"

class Jdb_kern_info_memory : public Jdb_kern_info_module
{};

static Jdb_kern_info_memory k_m INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_memory::Jdb_kern_info_memory()
  : Jdb_kern_info_module('m', "kmem_alloc::debug_dump")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_memory::show()
{
  ((Kmem_alloc*)Kmem_alloc::allocator())->debug_dump();
  typedef Kmem_slab::Reap_list::Const_iterator Iter;

  // Slab allocators
  for (Iter alloc = Kmem_slab::reap_list.begin();
       alloc != Kmem_slab::reap_list.end(); ++alloc)
    alloc->debug_dump();
}


