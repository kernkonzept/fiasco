INTERFACE:

#include "initcalls.h"
#include "types.h"

class Jdb_symbol_info;
class Jdb_lines_info;

class Jdb_dbinfo
{
};


//---------------------------------------------------------------------------
IMPLEMENTATION:

#include "config.h"

// We have to do this here because Jdb_symbol and Jdb_lines must not depend
// on Kmem_alloc.
PRIVATE static inline NOEXPORT
void
Jdb_dbinfo::init_symbols_lines ()
{
  Mword p;

  p = (sizeof(Jdb_symbol_info)*Jdb_symbol::Max_tasks) >> Config::PAGE_SHIFT;
  Jdb_symbol::init(Kmem_alloc::allocator()
      ->unaligned_alloc(p*Config::PAGE_SIZE), p);
  p = (sizeof(Jdb_lines_info) *Jdb_lines::Max_tasks)  >> Config::PAGE_SHIFT;
  Jdb_lines::init(Kmem_alloc::allocator()
      ->unaligned_alloc(p*Config::PAGE_SIZE), p);
}


//---------------------------------------------------------------------------
IMPLEMENTATION[ia32,amd64]:

#include "cpu_lock.h"
#include "jdb_lines.h"
#include "jdb_symbol.h"
#include "kmem.h"
#include "kmem_alloc.h"
#include "mem_layout.h"
#include "mem_unit.h"
#include "paging.h"
#include "space.h"
#include "static_init.h"

const Address  area_start  = Mem_layout::Jdb_debug_start;
const Address  area_end    = Mem_layout::Jdb_debug_end;
const unsigned area_size   = area_end - area_start;
const unsigned bitmap_size = (area_size / Config::PAGE_SIZE) / 8;

// We don't use the amm library here anymore since it is nearly impossible
// to debug it and I got some strange behavior. Instead of this we use a
// simple bitfield here that takes 2k for a virtual memory size of 64MB
// which is enough for the Jdb debug info. Speed for allocating/deallocating
// pages is not an issue here.
static unsigned char bitmap[bitmap_size];

STATIC_INITIALIZE(Jdb_dbinfo);

//---------------------------------------------------------------------------
IMPLEMENTATION[ia32, amd64]:

PUBLIC static FIASCO_INIT
void
Jdb_dbinfo::init()
{
  Address addr;

  for (addr = area_start; addr < area_end; addr += Config::SUPERPAGE_SIZE)
    Kmem::kdir->walk(Virt_addr(addr), Pdir::Depth, false, pdir_alloc(Kmem_alloc::allocator()));

  init_symbols_lines();
}


PRIVATE static
Address
Jdb_dbinfo::reserve_pages(unsigned pages)
{
  auto guard = lock_guard(cpu_lock);

  Unsigned8 *ptr, bit;

  for (ptr=bitmap, bit=0; ptr<bitmap+bitmap_size;)
    {
      Unsigned8 *ptr1, bit1, c;
      unsigned pages1;

      for (ptr1=ptr, bit1=bit, pages1=pages;;)
	{
	  if (ptr1>=bitmap+bitmap_size)
	    return 0;

	  c = *ptr1 & (1<<bit1);

	  if (++bit1 >= 8)
	    {
    	      bit1 = 0;
	      ptr1++;
	    }

	  if (c)
	    {
	      ptr = ptr1;
	      bit = bit1;
	      break;
	    }

	  if (!--pages1)
	    {
	      // found area -- make it as reserved
	      for (ptr1=ptr, bit1=bit, pages1=pages; pages1>0; pages1--)
		{
		  *ptr1 |= (1<<bit1);
		  if (++bit1 >= 8)
		    {
		      bit1 = 0;
		      ptr1++;
		    }
		}
	      return area_start + Config::PAGE_SIZE * (8*(ptr-bitmap) + bit);
	    }
	}
    }

  return 0;
}

PRIVATE static
void
Jdb_dbinfo::return_pages(Address addr, unsigned pages)
{
  auto guard = lock_guard(cpu_lock);

  unsigned nr_page = (addr-area_start) / Config::PAGE_SIZE;
  Unsigned8 *ptr = bitmap + nr_page/8, bit = nr_page % 8;

  for (; pages && ptr < bitmap+bitmap_size; pages--)
    {
      assert (*ptr & (1<<bit));
      *ptr &= ~(1<<bit);
      if (++bit >= 8)
	{
	  bit = 0;
	  ptr++;
	}
    }
}

//---------------------------------------------------------------------------
IMPLEMENTATION[ia32, amd64]:

PUBLIC static
bool
Jdb_dbinfo::map(Address phys, size_t &size, Address &virt)
{
  Address offs  = phys & ~Config::PAGE_MASK;

  size = (offs + size + Config::PAGE_SIZE - 1) & Config::PAGE_MASK;
  virt = reserve_pages (size / Config::PAGE_SIZE);
  if (!virt)
    return false;

  phys &= Config::PAGE_MASK;

  Kmem::kdir->map(phys, Virt_addr(virt), Virt_size(size),
      Pt_entry::Valid | Pt_entry::Writable | Pt_entry::Referenced
      | Pt_entry::Dirty, Pdir::Depth, false, Ptab::Null_alloc());

  virt += offs;
  return true;
}

PUBLIC static
void
Jdb_dbinfo::unmap(Address virt, size_t size)
{
  if (virt && size)
    {
      virt &= Config::PAGE_MASK;

      Kmem::kdir->unmap(Virt_addr(virt), Virt_size(size), Pdir::Depth, false);
      Mem_unit::tlb_flush();

      return_pages(virt, size/Config::PAGE_SIZE);
    }
}

PUBLIC static
void
Jdb_dbinfo::set(Jdb_symbol_info *sym, Address phys, size_t size)
{
  Address virt;

  if (!sym)
    return;

  if (!phys)
    {
      sym->get (virt, size);
      if (! virt)
	return;

      unmap (virt, size);
      sym->reset ();
      return;
    }

  if (! map (phys, size, virt))
    return;

  if (! sym->set (virt, size))
    {
      unmap (virt, size);
      sym->reset ();
    }
}

PUBLIC static
void
Jdb_dbinfo::set(Jdb_lines_info *lin, Address phys, size_t size)
{
  Address virt;

  if (!lin)
    return;

  if (!phys)
    {
      lin->get(virt, size);
      if (! virt)
	return;

      unmap(virt, size);
      lin->reset ();
    }

  if (!map(phys, size, virt))
    return;

  if (!lin->set(virt, size))
    {
      unmap(virt, size);
      lin->reset();
    }
}


//---------------------------------------------------------------------------
IMPLEMENTATION[ux]:

// No special mapping required for UX since all physical memory is mapped

#include "jdb_lines.h"
#include "jdb_symbol.h"
#include "kmem_alloc.h"
#include "mem_layout.h"
#include "static_init.h"

STATIC_INITIALIZE(Jdb_dbinfo);

PUBLIC static
void
Jdb_dbinfo::init()
{
  init_symbols_lines();
}

PUBLIC static
void
Jdb_dbinfo::set(Jdb_symbol_info *sym, Address phys, size_t size)
{
  if (!sym)
    return;

  if (!phys)
    sym->reset();
  else
    sym->set(Mem_layout::phys_to_pmem(phys), size);
}

PUBLIC static
void
Jdb_dbinfo::set(Jdb_lines_info *lin, Address phys, size_t size)
{
  if (!lin)
    return;

  if (!phys)
    lin->reset();
  else
    lin->set(Mem_layout::phys_to_pmem(phys), size);
}
