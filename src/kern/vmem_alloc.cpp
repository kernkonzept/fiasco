INTERFACE:

#include "paging.h"

class Mem_space;

class Vmem_alloc
{
public:

  enum Zero_fill {
    NO_ZERO_FILL = 0,
    ZERO_FILL,		///< Fill the page with zeroes.
  };

  enum
  {
    Kernel = 0,
    User = 1
  };

  static void init();

  static void *page_unmap(void *page);

  /**
   * Allocate a page of kernel memory and insert it into the master
   * page directory.
   *
   * @param address the virtual address where to map the page.
   * @param zf zero fill or zero map.
   * @param pa page attributes to use for the page table entry.
   */
  static void *page_alloc(void *address,
			  Zero_fill zf = NO_ZERO_FILL,
			  unsigned mode = Kernel);

private:
  static void page_map(void *address, int order, Zero_fill zf,
                       Address phys);
};

