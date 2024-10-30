INTERFACE:

class Jdb_util
{
public:
  static bool is_mapped(void const *addr);
};

IMPLEMENTATION:

IMPLEMENT_DEFAULT inline
bool
Jdb_util::is_mapped(void const * /*addr*/)
{
  return true;
}

IMPLEMENTATION[ia32 || amd64]:

#include "kmem.h"

IMPLEMENT_OVERRIDE
bool
Jdb_util::is_mapped(void const *x)
{
  return Kmem::virt_to_phys(x) != ~0UL;
}

IMPLEMENTATION[arm]:

#include "kmem.h"

IMPLEMENT_OVERRIDE inline NEEDS["kmem.h"]
bool
Jdb_util::is_mapped(void const* addr)
{
  return Kmem::kdir->virt_to_phys(reinterpret_cast<Address>(addr))
         != Invalid_address;
}

IMPLEMENTATION[sparc]:

IMPLEMENT_OVERRIDE inline
bool
Jdb_util::is_mapped(void const * /*addr*/)
{
  return false; // TBD
}
