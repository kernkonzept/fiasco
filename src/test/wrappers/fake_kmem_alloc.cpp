INTERFACE:

#include <flux/x86/types.h>	// vm_offset_t & friends

/** The kernel-memory allocator. 
    This allocator doesn't care about fragmentation or speed, so better 
    do not use it for fine-grained stuff.  Instead, use a kmem_slab_t.
 */
class kmem_alloc
{
public:
  /** Zero-fill mode for page_alloc(). */
  enum zero_fill_t { no_zero_fill = 0, zero_fill, zero_map };

private:
//   friend class jdb;

//   static vm_offset_t zero_page;

};

IMPLEMENTATION:

#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <flux/page.h>
#include "unix_aligned_alloc.h"

// vm_offset_t kmem_alloc::zero_page;

/** Allocate one page (with page alignment).
    @return address of memory block.  0 if no memory available
 */
PUBLIC static
void *
kmem_alloc::page_alloc()	// allocate a page at some kernel virtual addr
{
  return unix_aligned_alloc (PAGE_SIZE, PAGE_SIZE);
}

/** Allocate one page (with page alignment) at specific virtual address.
    This version of page_alloc() is equivalent to the simple
    page_alloc(), except that a virtual address can be specified, and
    that it can map in a shared zero-page that is unshared on write (in
    thread_t::handle_page_fault()).
    @param va kernel-virtual address at which to map the page
    @param do_zero_fill specifies whether to zero-fill or zero-map the page
    @return address of memory block.  0 if no memory available
    @pre page_aligned(va)
 */
PUBLIC static
void *
kmem_alloc::page_alloc(vm_offset_t va, zero_fill_t do_zero_fill = no_zero_fill)
{
  void* page = unix_aligned_alloc (PAGE_SIZE, PAGE_SIZE);
  if (page 
      && do_zero_fill != no_zero_fill)
    {
      memset (page, 0, PAGE_SIZE);
    }

  return page;
}

/** Free a page allocated with page_alloc().
    This function works for both version of page_alloc().  If the page
    is at a user-specified virtual address, that virtual address is cleared
    in the kernel's master page directory, and the TLB is flushed.
    page_free() does not care about task page tables, though.
 */
/* XXX why do we flush the TLB even though we don't care about task 
       page tables? */
PUBLIC static
void kmem_alloc::page_free(void *page)	// free page at specified kva
{
  unix_aligned_free (page, PAGE_SIZE);
}

/** Free page at given memory address. */
// static
// void 
// kmem_alloc::page_free_phys(void *page)
// {
//   helping_lock_guard_t guard(&lmm_lock);
//   lmm_free_page(&lmm, page);
// }

/** Allocate some bytes from the kernel's free store.  
    @param size size of requested memory block 
    @return address memory block.  0 if no memory is available
 */
PUBLIC static
void *kmem_alloc::alloc(vm_size_t size)
{
  return malloc(size);
}

/** Free a block of memory allocated with alloc() and return it 
    to the kernel's free store.
    @param block address of memory block
    @param size size of memory block as requested with alloc()
 */
PUBLIC static
void 
kmem_alloc::free(void *block, vm_size_t size)
{
  ::free (block);
}
