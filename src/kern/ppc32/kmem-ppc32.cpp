INTERFACE [ppc32]:

#include "paging.h"

class Page_table;

EXTENSION class Kmem
{
public:
  static Mword *kernel_sp();
  static void kernel_sp(Mword *);

  static Address virt_to_phys(const void *addr);
private:
  static Mword *_sp;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [ppc32]:

#include "paging.h"
#include "panic.h"

union {
  Kpdir kpdir;
  char storage[sizeof(Pdir)];
} kernel_page_directory;

DEFINE_GLOBAL_CONSTINIT
Global_data<Kpdir *> Kmem::kdir(&kernel_page_directory.kpdir);

Mword *Kmem::_sp = 0;

IMPLEMENT inline
Mword *Kmem::kernel_sp()
{ return _sp;}

IMPLEMENT inline
void Kmem::kernel_sp(Mword *sp)
{ _sp = sp; }

IMPLEMENT inline NEEDS["paging.h"]
Address Kmem::virt_to_phys(const void *addr)
{
  Address a = reinterpret_cast<Address>(addr);
  return kdir->virt_to_phys(a);
}

IMPLEMENT inline
bool Kmem::is_kmem_page_fault(Mword pfa, Mword /*error*/)
{
  return in_kernel(pfa);
}
