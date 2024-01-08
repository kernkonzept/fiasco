IMPLEMENTATION [ia32]:

#include "assert.h"
#include "paging_bits.h"

PRIVATE static inline FIASCO_INIT_CPU
void
Kmem::init_cpu_arch(Cpu &cpu, Lockless_alloc *cpu_mem)
{
  // Allocate the Task State Segment for the double fault handler.
  // Just the context part of the TSS is required, the IO bitmap is not needed.
  Tss *tss_dbf = cpu_mem->alloc_bytes<Tss>(sizeof(Tss::Ctx), Order(4));

  // Check that the TSS for the double fault handler has been allocated and
  // that it does not cross the page boundary.
  assert(tss_dbf != nullptr);
  assert(Pg::trunc(reinterpret_cast<Address>(tss_dbf))
         == Pg::trunc(reinterpret_cast<Address>(tss_dbf) + sizeof(Tss::Ctx)));

  cpu.init_tss_dbf(tss_dbf, Mem_layout::pmem_to_phys(Kmem::dir()));
  cpu.init_sysenter();
}
