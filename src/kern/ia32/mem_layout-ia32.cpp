INTERFACE [ia32 || amd64]:

#include "tss.h"

EXTENSION class Mem_layout
{
public:
  enum : size_t
  {
    Tss_mem_size = Config::Max_num_cpus * sizeof(Tss)
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:

#include "paging_bits.h"

static_assert(Address{Mem_layout::Tss_start} + Address{Mem_layout::Tss_mem_size}
              < Address{Mem_layout::Tss_end},
              "Too many CPUs configured, not enough space to map TSSs.");
