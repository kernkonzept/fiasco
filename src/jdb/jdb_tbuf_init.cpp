INTERFACE:

#include "jdb_tbuf.h"

class Jdb_tbuf_init : public Jdb_tbuf
{
private:
  static unsigned max_size();
  static unsigned allocate(unsigned size);

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
#include "vmem_alloc.h"

STATIC_INITIALIZE_P(Jdb_tbuf_init, JDB_MODULE_INIT_PRIO);

IMPLEMENT_DEFAULT FIASCO_INIT
unsigned
Jdb_tbuf_init::max_size()
{ return Mem_layout::Tbuf_buffer_size; }

IMPLEMENT_DEFAULT FIASCO_INIT
unsigned
Jdb_tbuf_init::allocate(unsigned size)
{
  if (size > max_size())
    return max_size();

  _status = (Tracebuffer_status *)Mem_layout::Tbuf_status_page;
  if (!Vmem_alloc::page_alloc((void*) status(), Vmem_alloc::ZERO_FILL))
    panic("jdb_tbuf: alloc status page at %p failed", _status);

  _buffer = (Tb_entry_union *)Mem_layout::Tbuf_buffer_area;
  Address va = (Address) buffer();
  for (unsigned i = 0; i < size / Config::PAGE_SIZE;
       ++i, va += Config::PAGE_SIZE)
    if (!Vmem_alloc::page_alloc((void *)va, Vmem_alloc::NO_ZERO_FILL))
      return i * Config::PAGE_SIZE;

  return size;
}

// init trace buffer
IMPLEMENT FIASCO_INIT
void Jdb_tbuf_init::init()
{
  //static Irq_sender tbuf_irq(Config::Tbuf_irq);

  static int init_done;

  if (!init_done)
    {
      init_done = 1;

      unsigned n;
      unsigned want_entries = Config::tbuf_entries;

      if (Koptions::o()->opt(Koptions::F_tbuf_entries))
        want_entries = Koptions::o()->tbuf_entries;

      // Must be a power of 2 (for performance reasons). Also use a sane upper
      // limit which fits into 4GB. The allocate function limits the buffer to
      // the available window in the virtual memory layout.
      for (n = Config::PAGE_SIZE / sizeof(Tb_entry_union);
           n < want_entries && n < max_size();
           n <<= 1)
        ;

      if (n < want_entries)
        panic("Cannot allocate more than %u entries for tracebuffer.\n", n);

      max_entries(n);
      unsigned size = n * sizeof(Tb_entry_union);
      unsigned got = allocate(size);
      if (got < size)
        panic("Could not allocate trace buffer memory entries=%u: got %u/%u KiB.\n",
              n, got >> 10, size >> 10);

      status()->scaler_tsc_to_ns = Cpu::boot_cpu()->get_scaler_tsc_to_ns();
      status()->scaler_tsc_to_us = Cpu::boot_cpu()->get_scaler_tsc_to_us();
      status()->scaler_ns_to_tsc = Cpu::boot_cpu()->get_scaler_ns_to_tsc();

      _tbuf_max    = buffer() + max_entries();
      _size        = size;

      clear_tbuf();
    }
}
