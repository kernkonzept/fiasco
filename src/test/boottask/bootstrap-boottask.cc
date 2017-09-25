/* this code is run directly from boot.S.  our task is to setup the
   paging just enough so that L4 can run in its native address space 
   at 0xf0001000, and then start up L4.  */

#include <cassert>
#include <cstring>
#include <flux/x86/multiboot.h>
#include <flux/x86/base_paging.h>
#include <flux/x86/base_cpu.h>
#include <flux/x86/pc/phys_lmm.h>

extern "C" void crt0_start(struct multiboot_info *mbi, unsigned int flag) 
  __attribute__((noreturn));
extern "C" void boottask_entry()  __attribute__((noreturn));

extern char kernel_start, kernel_end, boottask_start, boottask_end;

extern "C" void bootstrap(struct multiboot_info *mbi, unsigned int flag) 
  __attribute__((noreturn));

extern "C" void
bootstrap(struct multiboot_info *mbi, unsigned int flag)
{
  assert(flag == MULTIBOOT_VALID);

  // setup stuff for base_paging_init() 
  base_cpu_setup();
  phys_mem_max = 1024 * (1024 + mbi->mem_upper);

  // now do base_paging_init(): sets up paging with one-to-one mapping
  base_paging_init();

  // map in physical memory at 0xf0000000
  pdir_map_range(base_pdir_pa, 0xf0000000, 0, phys_mem_max,
		 INTEL_PTE_VALID | INTEL_PDE_WRITE | INTEL_PDE_USER);


  // copy l4 kernel binary to its load address
  memcpy((void *) crt0_start, &kernel_start, 
	 &kernel_end - &kernel_start);

  memcpy((void *) boottask_entry, &boottask_start, 
	 &boottask_end - &boottask_start);

  crt0_start(mbi, flag);
}

/* the following simple-minded function definition overrides the (more
   complicated) default one the OSKIT*/
extern "C" int 
ptab_alloc(vm_offset_t *out_ptab_pa)
{
  static char pool[0x100000];
  static vm_offset_t pdirs;
  static int initialized = 0;

  if (! initialized)
    {
      initialized = 1;

      pdirs = (reinterpret_cast<vm_offset_t>(&pool[0]) + PAGE_SIZE - 1) 
	& ~PAGE_MASK;
    }

  assert(pdirs < reinterpret_cast<vm_offset_t>(&pool[0]) + sizeof(pool));

  *out_ptab_pa = pdirs;
  pdirs += PAGE_SIZE;

  return 0;
}
