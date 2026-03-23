IMPLEMENTATION [mips]:

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "kmem_alloc.h"
#include "config.h"
#include "cpu.h"
#include "jdb_ktrace.h"

IMPLEMENT_OVERRIDE FIASCO_INIT
size_t
Jdb_tbuf_init::max_size()
{ return 2 << 20; }

IMPLEMENT_OVERRIDE FIASCO_INIT
size_t
Jdb_tbuf_init::allocate(size_t entries)
{
  size_t alloc = size(entries);
  if (alloc > max_size())
    return capacity(max_size());

  _status =
    static_cast<Tracebuffer_status *>(
      Kmem_alloc::allocator()->alloc(Bytes(sizeof(Tracebuffer_status))));
  if (!_status)
    panic("jdb_tbuf: Allocation of tracebuffer status page failed");

  _slots = reinterpret_cast<Tracebuffer_slot *>(Kmem_alloc::allocator()
                                                ->alloc(Bytes(alloc)));
  if (!_slots)
    return 0;

  memset(_status, 0, sizeof(Tracebuffer_status));
  memset(_slots, 0, alloc);
  return entries;
}

IMPLEMENT_OVERRIDE
bool
Jdb_tbuf_init::in_pmem()
{ return true; }
