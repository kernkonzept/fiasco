/*
 * PPC hashed page table 
 */
INTERFACE [ppc32]:

#include "types.h"
#include "initcalls.h"

EXTENSION class Mem_space
{

  public:
    static void init();
  private:
    typedef struct Evict {
	Address virt;
	Address phys;
	Dir_type *dir;
    } Evict;

    static Mword _htabmask;
    static Mword _htaborg;

    Status v_insert_htab(Address phys, Address virt, Address *pte_ptr,
                         Evict *evict);
    unsigned long v_delete_htab(Address pte_ptr, unsigned page_attribs);
};

IMPLEMENTATION [ppc32]:

#include <cstdio>
#include "config.h"
#include "kip_init.h"
#include "panic.h"
#include "util.h"

Mword Mem_space::_htabmask = 0;
Mword Mem_space::_htaborg  = 0;

// ea == virt - effective address (32 bit)
// pa - physical address

//------------------------------------------------------------------------------
/* 
 * Page-table handling
 */

PRIVATE static inline
Address
Mem_space::page_index(Address ea)
{
  return (ea >> 12) & 0xffff;
}

PRIVATE static inline
Address
Mem_space::_vsid(Address ea, Pdir *dir)
{
  //24 bits of pdir + segment selector of ea
  return ((Address)dir >> 8) + (ea >> 28);
}

PRIVATE inline NEEDS[Mem_space::_vsid]
Address
Mem_space::vsid(Address ea)
{
  Dir_type *dir = _dir;

  //kernel space
  if(ea > Mem_layout::User_max)
    dir = Kmem::kdir;

  return _vsid(ea, dir);
}

PRIVATE static inline
Mem_space::Dir_type*
Mem_space::vsid_to_dir(Mword vsid)
{
  vsid &= ~0xf;
  vsid <<= 8;
  return reinterpret_cast<Dir_type*>(vsid);
}

IMPLEMENT inline NEEDS["cpu.h", Mem_space::vsid]
void
Mem_space::make_current()
{
  Cpu::set_vsid(vsid(0));
  _current.cpu(current_cpu()) = this;
}

PRIVATE static inline
Address
Mem_space::hash_to_pteg(Address hash)
{
  // high seven htaborg bits
  Mword pteg = _htaborg & 0xfe000000;
  //htabmask shifted 10
  hash &= (_htabmask << 10) | 0x3ff;
  //low nine htaborg bits
  hash |= (_htaborg & 0x01ff0000) >> 6;
  return pteg | (hash << 6);
}

PRIVATE inline NEEDS[Mem_space::vsid]
Address
Mem_space::hash_func(Address ea)
{
 return (vsid(ea) & 0x7ffff) ^ page_index(ea);
}

/**
 * Find page-table entry group
 *
 * primary and secondary pteg
 */
PRIVATE inline
Address *
Mem_space::pteg_p(Address ea)
{
  return reinterpret_cast<Address*>(hash_to_pteg(hash_func(ea)));
}

PRIVATE inline
Address *
Mem_space::pteg_s(Mword ea)
{
  Address hash_s = (~hash_func(ea)) & 0x7ffff;
  return reinterpret_cast<Address*>(hash_to_pteg(hash_s));
}

PRIVATE 
Pte_htab *
Mem_space::locate(Pte_htab *pte, Address ea) 
{
  Address *pteg[2] = {pteg_p(ea), pteg_s(ea)};
  Pte_htab *ret = NULL, *free = NULL;
  Mword h = 0;
  for(int i = 0; i < 2*Config::Htab_entries; i += 2)
    {
      for(int j = 0; j <= 1; j++)
	{
	  ret = reinterpret_cast<Pte_htab*>(pteg[j] + i);
	  pte->pte.h = j;

	  if(pte->v_equal(ret))
	    return ret;

	  if(!ret->valid() && !free) 
	    {
	      free = ret;
	      h = j;
	    }
	}

      if(free) break;
    }

  if(EXPECT_FALSE(!free))
    free = ret;
  pte->pte.h = h;
  return free;
}

//------------------------------------------------------------------------------
PRIVATE template< typename T >
inline
T
Mem_space::to_htab_fmt(T page_attribs)
{
  // if page attribs has the writable bit set, make sure to erase the readable
  // bit because 0x11b is read-only on ppc
#ifdef FIX_THIS
  if (page_attribs & Page_writable)
    page_attribs &= ~Page_user_accessible;
#endif

  return page_attribs & (~Pt_entry::Valid & ~Pt_entry::Htab_entry);
}

PRIVATE template< typename T >
inline
T
Mem_space::to_kernel_fmt(T page_attribs, bool is_htab_entry)
{
  T attribs = page_attribs;

#ifdef FIX_THIS
  attribs &= Page_all_attribs;

  if(!is_htab_entry)
    attribs |=  Pte_ptr::Valid;

  if(attribs & Page_writable)
    attribs |= Page_user_accessible;
#endif

  return attribs;
}

PRIVATE
Mem_space::Status
Mem_space::pte_attrib_upgrade(Pte_ptr *e, size_t size, unsigned page_attribs)
{
  (void)e; (void)size; (void)page_attribs;

  Status ret = Insert_warn_attrib_upgrade;
#ifdef FIX_THIS
  Pte_ptr *e2 = e;
  page_attribs = to_htab_fmt(page_attribs);

  for(Address offs = 0; offs < (size / Config::PAGE_SIZE) * sizeof(Mword);
      offs += sizeof(Mword))
    {
      e2 = reinterpret_cast<Pt_entry *>(e2 + offs);

      if(!e2->valid())
        continue;

      if(e2->is_htab_entry())
        {
          if(EXPECT_FALSE((e2->raw() | page_attribs) == e2->raw()))
            {
              ret =  Insert_warn_exists;
              continue;
            }
          e2->add_attr(page_attribs);
          ret = Insert_warn_attrib_upgrade;
        }
      else
        ret = pte_attrib_upgrade(e2->raw(), page_attribs);
    }
#endif

  return ret;
}


PRIVATE inline
Mem_space::Status
Mem_space::pte_attrib_upgrade(Address pte_addr, unsigned page_attribs)
{
  auto guard = lock_guard(cpu_lock);
  Pte_htab * pte_phys = Pte_htab::addr_to_pte(pte_addr);
  if(EXPECT_FALSE((pte_phys->phys() | page_attribs) 
	 	  == pte_phys->phys()))
    return Insert_warn_exists;

  pte_phys->pte.valid = 0;
  Mem_unit::sync();
  pte_phys->raw.raw1 = to_htab_fmt(pte_phys->raw.raw1 | page_attribs);
  Mem_unit::tlb_flush(pte_phys->pte_to_ea());
  pte_phys->pte.valid = 1;
  Mem_unit::sync();

/*
  printf("DBG: %s: phys %08lx virt: %08lx vsid: %lx raw0: %08lx raw1: %08lx ptr %lx\n",
          __func__, pte_phys->phys() & Config::PAGE_MASK, pte_phys->pte_to_ea(), 
          vsid(pte_phys->pte_to_ea()), pte_phys->virt(), pte_phys->phys(), pte_phys);
*/
  return Insert_warn_attrib_upgrade;
}

IMPLEMENT
Mem_space::Status
Mem_space::v_insert_htab(Address phys, Address virt,
                         Address *pte_ptr, Evict *evict)
{
  Pte_htab pte(to_htab_fmt(phys), virt,
               vsid(virt));
  Pte_htab *pte_phys = locate(&pte, virt);
/*
  if(EXPECT_FALSE(pte.v_equal(pte_phys)
     && !pte.p_equal(pte_phys)))
     return Insert_err_exists;
*/
  //set pte pointer
  *pte_ptr = reinterpret_cast<Address>(pte_phys);

  //we have to evict something
  if(EXPECT_FALSE(pte_phys->valid()))
    {
      evict->virt  = pte_phys->pte_to_ea();
      evict->phys  = pte_phys->raw.raw1 | Pt_entry::Valid;
      evict->dir   = vsid_to_dir(pte_phys->pte.vsid);
      *pte_phys = pte;
      //flush tlb in case the page is in current address space
      Mem_unit::tlb_flush(evict->virt);
      Mem_unit::sync();
      return Insert_err_nomem;;
    }

  *pte_phys = pte;
  Mem_unit::sync();


//DEBUG
#if 0
  printf("htab insert %s: phys %08lx virt: %08lx vsid: %lx raw0: %08lx raw1: %08lx ptr %lx\n",
          __func__, phys, virt, vsid(virt), pte.virt(), pte.phys(), *pte_ptr);
  printf("pte_addr: %p\n", pte_phys);
  printf("vsid: %lx, dir %p, rev_dir %p\n", vsid(virt), _dir, vsid_to_dir(vsid(virt)));
  printf("reverse virt: %lx\n", pte_to_ea(pte_phys));
  printf("raw0 %lx raw1 %lx\n", pte_phys->virt(), pte_phys->phys());
  printf("current pdir %p\n", current_pdir());
  printf("MBAR %lx\n", *((Address*)0x80000000));
  printf("vsid0 %lx\n", vsid(0));
  make_current();
  for(int i = 0; i < 16; i++) {
    printf("vsid%d read %lx\n", i, Cpu::read_vsid(i));
  }
  v_lookup_htab(virt, 0, 0);
  v_delete_htab(virt, Page_user_accessible);
//END debug
#endif

  return Insert_ok;
}


IMPLEMENT
unsigned long
Mem_space::v_delete_htab(Address pte_addr, unsigned page_attribs = Page_all_attribs)
{
  (void)pte_addr; (void)page_attribs;
#ifdef FIX_THIS
  auto guard = lock_guard(cpu_lock);
  unsigned long ret;
  Pte_htab *pte_phys = Pte_htab::addr_to_pte(pte_addr);

  if(EXPECT_FALSE(!pte_phys->valid()))
    return 0;

  ret = pte_phys->phys() & page_attribs;

  if(!(page_attribs & Page_user_accessible))
    pte_phys->raw.raw0 &= ~page_attribs;
  else
      pte_phys->raw.raw0 = pte_phys->raw.raw1 = 0;

  Mem_unit::tlb_flush(pte_phys->pte_to_ea());
  Mem_unit::sync();
  return ret;
#endif
  return 0;
}

IMPLEMENT inline NEEDS["kmem.h"]
void
Mem_space::switchin_context(Mem_space *from)
{
  if(dir() == Kmem::kdir)
    return;

  if (this != from)
    make_current();
}

//------------------------------------------------------------------------------
/*
 * INIT FUNCITONS 
 */

/**
 * Calculate htab size, as described in PPC reference
 * manual, based on given memory
 */
PRIVATE static FIASCO_INIT
unsigned long
Mem_space::htab_size(Mword ram)
{
  ram >>= 7;
  Mword log2 = Util::log2(ram);

  if(ram != 1UL << log2)
    log2++;

  //min is 64 KB
  log2 = (log2 < 16)? 16: log2;
  return 1UL << log2;
}

/**
 * Find physical memory area.
 *
 * alloc_size has to be a multiple of two, result will be naturally aligned
 */
PRIVATE static FIASCO_INIT
Mword
Mem_space::alloc(Mword alloc_size)
{
  //unsigned long max = ~0UL;
  Mword start;
  for (;;)
    {
      Mem_region r;    r.start=2; r.end=1;// = Kip::k()->last_free(max);
      if (r.start > r.end)
        panic("Corrupt memory descscriptor in KIP...");

      if (r.start == r.end)
        panic("not enough memory for page table");

      //max = r.start;
      Mword size  = r.end - r.start + 1;
      start = (r.end - alloc_size + 1) & ~(alloc_size - 1);
      if(alloc_size <= size && start >= r.start)
	{
	  Kip::k()->add_mem_region(Mem_desc(start, r.end,
					    Mem_desc::Reserved));
	  break;
	}
    }
  return start;
}

/**
 * Install htab
 */
PRIVATE static FIASCO_INIT
void
Mem_space::install()
{
  Mword kernel_vsid = _vsid(Mem_layout::User_max + 1, Kmem::kdir)
                      | Segment::Default_attribs;

  asm volatile( " sync              \n"
		" mtsdr1 %[sdr1]    \n" //set SDR1
		" isync             \n"
		" mtsr 15, %[vsid]  \n" //set kernel space
		                        //(last 256 MB of address space)
		:
		: [sdr1]"r" (_htaborg | _htabmask),
		  [vsid]"r" (kernel_vsid)
		);
}

IMPLEMENT static FIASCO_INIT
void
Mem_space::init()
{
   Mword alloc_size = 0; // tbd: get size from memory descriptors
   _htaborg  = alloc(alloc_size);
   _htabmask = ((alloc_size >> 16) - 1) & 0x1ff; //9 bit

   install();

   printf("Htab  installed at: [%08lx; %08lx] - %lu KB\n", 
          _htaborg, _htaborg + alloc_size - 1, alloc_size/1024);
}

