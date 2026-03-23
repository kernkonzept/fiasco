INTERFACE:

#include "jdb_tbuf.h"

class Jdb_tbuf_init : public Jdb_tbuf
{
  static size_t max_size();
  static size_t allocate(size_t entries);

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
      size_t want_entries = Config::tbuf_entries;

      if (Koptions::o()->opt(Koptions::F_tbuf_entries))
        want_entries = Koptions::o()->tbuf_entries;

      // The count of slots must be a power of 2 for performance reasons and
      // also because the slot index is determined by a modulo of the count
      // (i.e. binary anding it with count - 1).
      //
      // We use a sane upper limit which fits into 2 GiB. The max_size() method
      // also limits the buffer to the available window in the virtual memory
      // layout.

      size_t entries = capacity(Config::PAGE_SIZE);
      size_t max_entries = capacity(max_size());

      while (entries < want_entries && entries < max_entries)
        entries <<= 1;

      if (entries < want_entries)
        panic("Cannot allocate more than %zu entries for tracebuffer.",
              entries);

      size_t got_entries = allocate(entries);
      if (got_entries < entries)
        panic("Cannot allocate %zu entries for tracebuffer. Got %zu entries.",
              entries, got_entries);

      _capacity = entries;
      _mask = entries - 1;
      _filter_hidden = false;

      _status->version = Tracebuffer_version;
      _status->alignment = Config::stable_cache_alignment;
      _status->mask = _mask;
      _status->tail = Nil;

      _status->scaler_tsc_to_ns = Cpu::boot_cpu()->get_scaler_tsc_to_ns();
      _status->scaler_tsc_to_us = Cpu::boot_cpu()->get_scaler_tsc_to_us();
      _status->scaler_ns_to_tsc = Cpu::boot_cpu()->get_scaler_ns_to_tsc();

      clear_tbuf();
      _init_done = true;
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
Jdb_tbuf_init::allocate(size_t entries)
{
  size_t alloc = size(entries);
  assert(Pg::aligned(alloc));

  if (alloc > max_size())
    return capacity(max_size());

  _status
    = reinterpret_cast<Tracebuffer_status *>(Mem_layout::Tbuf_status_page);
  _slots = reinterpret_cast<Tracebuffer_slot *>(Mem_layout::Tbuf_buffer_area);

  if (!Vmem_alloc::page_alloc(_status, Vmem_alloc::ZERO_FILL))
    panic("jdb_tbuf: Allocation of tracebuffer status page at %p failed",
          static_cast<void *>(_status));

  for (size_t i = 0; i < Pg::count(alloc); ++i)
    {
      auto ptr = offset_cast<void *>(_slots, Pg::size(i));
      if (!Vmem_alloc::page_alloc(ptr, Vmem_alloc::ZERO_FILL))
        return capacity(Pg::size(i));
    }

  return entries;
}

// --------------------------------------------------------------------------
IMPLEMENTATION [!mmu]:

#include "kmem_alloc.h"

IMPLEMENT_DEFAULT FIASCO_INIT
size_t
Jdb_tbuf_init::max_size()
{ return size_t{2U} << 30; }

IMPLEMENT_DEFAULT FIASCO_INIT
size_t
Jdb_tbuf_init::allocate(size_t entries)
{
  size_t alloc = size(entries);
  if (alloc > max_size())
    return capacity(max_size());

  static Tracebuffer_status status;
  _status = &status;

  _slots = reinterpret_cast<Tracebuffer_slot *>(Kmem_alloc::allocator()
                                                ->alloc(Bytes(alloc)));
  if (!_slots)
    return 0;

  memset(_status, 0, sizeof(status));
  memset(_slots, 0, alloc);
  return entries;
}
