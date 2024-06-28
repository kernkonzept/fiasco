INTERFACE:

#include "mem_layout.h"
#include "global_data.h"
#include "paging.h"

class Kmem : public Mem_layout
{
public:
  static bool is_kmem_page_fault(Mword pfa, Mword error);
  static void kernel_remap();

  static Global_data<Kpdir *> kdir;
};

IMPLEMENTATION:

IMPLEMENT_DEFAULT
void
Kmem::kernel_remap()
{}
