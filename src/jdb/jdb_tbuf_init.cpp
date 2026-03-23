INTERFACE:

#include "jdb_tbuf.h"

class Jdb_tbuf_init : public Jdb_tbuf
{
  static size_t max_size();
  static size_t allocate(size_t size);

  static bool _init_done;

public:
  static void init();
};

IMPLEMENTATION:

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <panic.h>

#include "config.h"
#include "cpu.h"
#include "jdb_ktrace.h"
#include "koptions.h"
#include "mem_layout.h"

STATIC_INITIALIZE_P(Jdb_tbuf_init, JDB_MODULE_INIT_PRIO);

bool Jdb_tbuf_init::_init_done = false;

// init trace buffer
IMPLEMENT FIASCO_INIT
void Jdb_tbuf_init::init()
{
  if (!_init_done)
    {
      _init_done = true;

      size_t want_entries = Config::tbuf_entries;

      if (Koptions::o()->opt(Koptions::F_tbuf_entries))
        want_entries = Koptions::o()->tbuf_entries;

      static_assert((sizeof(Tb_entry_union) & (sizeof(Tb_entry_union) - 1)) == 0,
                    "Tb_entry_size must be a power of two");

      // Must be a power of 2 for performance reasons and because the buffer
      // pointer is determined by binary AND of size-1. Also use a sane upper
      // limit which fits into 4 GiB. The allocate function limits the buffer to
      // the available window in the virtual memory layout.
      size_t n;
      for (n = Config::PAGE_SIZE / sizeof(Tb_entry_union);
           n < want_entries && n < max_size() / sizeof(Tb_entry_union);
           n <<= 1)
        ;

      if (n < want_entries)
        panic("Cannot allocate more than %zu entries for tracebuffer.", n);

      size_t size = n * sizeof(Tb_entry_union);
      size_t got = allocate(size);
      if (got < size)
        panic("Could not allocate trace buffer memory entries=%zu: got %zu/%zu KiB.",
              n, got >> 10, size >> 10);

      status()->scaler_tsc_to_ns = Cpu::boot_cpu()->get_scaler_tsc_to_ns();
      status()->scaler_tsc_to_us = Cpu::boot_cpu()->get_scaler_tsc_to_us();
      status()->scaler_ns_to_tsc = Cpu::boot_cpu()->get_scaler_ns_to_tsc();

      _max_entries = n;
      _size        = size;

      clear_tbuf();
    }
}

// --------------------------------------------------------------------------
IMPLEMENTATION [mmu]:

#include "vmem_alloc.h"
#include "paging_bits.h"

IMPLEMENT_DEFAULT FIASCO_INIT
size_t
Jdb_tbuf_init::max_size()
{ return Mem_layout::Tbuf_buffer_size; }

IMPLEMENT_DEFAULT FIASCO_INIT
size_t
Jdb_tbuf_init::allocate(size_t size)
{
  assert(Pg::aligned(size));

  if (size > max_size())
    return max_size();

  _status =
    reinterpret_cast<Tracebuffer_status *>(Mem_layout::Tbuf_status_page);
  if (!Vmem_alloc::page_alloc(static_cast<void*>(status()),
                              Vmem_alloc::ZERO_FILL))
    panic("jdb_tbuf: alloc status page at %p failed",
          static_cast<void *>(_status));

  _buffer = reinterpret_cast<Tb_entry_union *>(Mem_layout::Tbuf_buffer_area);
  Address va = reinterpret_cast<Address>(buffer());
  for (size_t i = 0; i < Pg::count(size); ++i, va += Config::PAGE_SIZE)
    if (!Vmem_alloc::page_alloc(reinterpret_cast<void *>(va),
                                Vmem_alloc::NO_ZERO_FILL))
      return Pg::size(i);

  return size;
}

// --------------------------------------------------------------------------
IMPLEMENTATION [!mmu]:

#include "kmem_alloc.h"

IMPLEMENT_DEFAULT FIASCO_INIT
size_t
Jdb_tbuf_init::max_size()
{ return ~size_t{0U}; }

IMPLEMENT_DEFAULT FIASCO_INIT
size_t
Jdb_tbuf_init::allocate(size_t size)
{
  static Tracebuffer_status _tb_status;
  _status = &_tb_status;

  _buffer = reinterpret_cast<Tb_entry_union *>(Kmem_alloc::allocator()
                                                ->alloc(Bytes(size)));
  if (!_buffer)
    return 0;

  memset(_buffer, 0, size);

  return size;
}
