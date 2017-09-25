INTERFACE [arm && !hyp]:

#include "paging.h"

typedef Pdir Kpdir;

//---------------------------------------------------------------------
INTERFACE [arm && hyp]:

#include "paging.h"

class Kpdir
: public Ptab::Base<K_pte_ptr, Ptab_traits_vpn, Ptab_va_vpn>
{
public:
  enum { Super_level = Pte_ptr::Super_level };
  Address virt_to_phys(Address virt) const
  {
    Virt_addr va(virt);
    auto i = walk(va);
    if (!i.is_valid())
      return ~0;

    return i.page_addr() | cxx::get_lsb(virt, i.page_order());
  }
};

//---------------------------------------------------------------------
INTERFACE [arm]:

#include "mem_layout.h"

class Kmem_space : public Mem_layout
{
public:
  static void init();
  static void init_hw();
  static Kpdir *kdir();

private:
  static Kpdir *_kdir;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cassert>
#include <panic.h>

#include "console.h"
#include "kip_init.h"
#include "mem_unit.h"

#include <cstdio>

IMPLEMENT inline
Kpdir *Kmem_space::kdir()
{ return _kdir; }

// initialize the kernel space (page table)
IMPLEMENT
void Kmem_space::init()
{
  unsigned domains = 0x0001;

  asm volatile("mcr p15, 0, %0, c3, c0" : : "r" (domains));

  Mem_unit::clean_vdcache();
}

// always 16kB also for LPAE we use 4 consecutive second level tables
char kernel_page_directory[0x4000]
  __attribute__((aligned(0x4000), section(".bss.kernel_page_dir")));

//----------------------------------------------------------------------------------
IMPLEMENTATION[arm && !arm_lpae]:

Kpdir *Kmem_space::_kdir = (Kpdir *)&kernel_page_directory;

//----------------------------------------------------------------------------------
IMPLEMENTATION[arm && arm_lpae]:

Unsigned64 kernel_lpae_dir[4] __attribute__((aligned(4 * sizeof(Unsigned64))));
Kpdir *Kmem_space::_kdir = (Kpdir *)&kernel_lpae_dir;
