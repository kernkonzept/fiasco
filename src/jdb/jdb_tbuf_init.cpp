INTERFACE:

#include "jdb_tbuf.h"

class Jdb_tbuf_init : public Jdb_tbuf
{
private:
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
Jdb_tbuf_init::allocate(unsigned size)
{
  _status = (Tracebuffer_status *)Mem_layout::Tbuf_status_page;
  if (!Vmem_alloc::page_alloc((void*) status(), Vmem_alloc::ZERO_FILL,
                              Vmem_alloc::User))
    panic("jdb_tbuf: alloc status page at %p failed", _status);

  _buffer = (Tb_entry_union *)Mem_layout::Tbuf_buffer_area;
  Address va = (Address) buffer();
  for (unsigned i = 0; i < size / Config::PAGE_SIZE;
       ++i, va += Config::PAGE_SIZE)
    if (!Vmem_alloc::page_alloc((void *)va, Vmem_alloc::NO_ZERO_FILL,
                                Vmem_alloc::User))
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

      // minimum: 8KB (  2 pages), maximum: 2MB (512 pages)
      // must be a power of 2 (for performance reasons)
      for (n = Config::PAGE_SIZE / sizeof(Tb_entry_union);
           n < want_entries && n * sizeof(Tb_entry_union) < 0x200000;
           n <<= 1)
        ;

      if (n < want_entries)
        panic("Cannot allocate more than %u entries for tracebuffer.\n", n);

      max_entries(n);
      unsigned size = n * sizeof(Tb_entry_union);
      unsigned got = allocate(size);
      if (got == 0)
        panic("could not allocate trace buffer memory entries=%u %u KiB.\n",
              n, size >> 10);

      if (got & (got - 1))
        panic("Trace buffer size unaligned\n");

      if (got < size)
        {
          n = got / sizeof(Tb_entry_union);
          size = n * sizeof(Tb_entry_union);
          max_entries(n);
          printf("Shrinking trace buffer to %u entries.\n", n);
        }

      status()->window[0].tracebuffer = (Address)_buffer;
      status()->window[1].tracebuffer = (Address)_buffer + size / 2;
      status()->window[0].size        = size / 2;
      status()->window[1].size        = size / 2;
      status()->window[0].version     = 0;
      status()->window[1].version     = 0;

      status()->scaler_tsc_to_ns = Cpu::boot_cpu()->get_scaler_tsc_to_ns();
      status()->scaler_tsc_to_us = Cpu::boot_cpu()->get_scaler_tsc_to_us();
      status()->scaler_ns_to_tsc = Cpu::boot_cpu()->get_scaler_ns_to_tsc();

      _tbuf_max    = buffer() + max_entries();
      _count_mask1 =  max_entries()    - 1;
      _count_mask2 = (max_entries())/2 - 1;
      _size        = size;

      clear_tbuf();
    }
}
