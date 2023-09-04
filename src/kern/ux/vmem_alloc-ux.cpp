
IMPLEMENTATION[ux]:

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include "boot_info.h"
#include "paging_bits.h"
#include "panic.h"

PUBLIC static inline NEEDS [<cerrno>, <cstring>, <unistd.h>, <sys/mman.h>,
                            "boot_info.h", "paging_bits.h", "panic.h"]
void
Vmem_alloc::page_map(void *address, int order, Zero_fill zf, Address phys)
{
  if (mmap(address, Pg::size(1 << order),
           PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED,
           Boot_info::fd(), phys) == MAP_FAILED)
    panic("mmap error: %s", strerror(errno));

  if (zf == ZERO_FILL)
    memset(address, 0, Pg::size(1 << order));
}
