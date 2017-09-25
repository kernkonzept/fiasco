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
unsigned
Jdb_tbuf_init::allocate(unsigned size)
{
  _status = (Tracebuffer_status *)Kmem_alloc::allocator()
    ->unaligned_alloc(sizeof (Tracebuffer_status));
  if (!_status)
    return 0;

  _buffer = (Tb_entry_union *)Kmem_alloc::allocator()
    ->unaligned_alloc(size);

  if (!_buffer)
    return 0;

  return size;
}

