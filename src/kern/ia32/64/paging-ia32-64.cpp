INTERFACE[amd64]:

#include <cassert>
#include "types.h"
#include "config.h"
#include "mem_layout.h"
#include "ptab_base.h"

class PF {};
class Page {};


class Pt_entry
{
public:
  enum
  {
    Super_level   = 2,
    Valid         = 0x00000001LL, ///< Valid
    Writable      = 0x00000002LL, ///< Writable
    User          = 0x00000004LL, ///< User accessible
    Write_through = 0x00000008LL, ///< Write through
    Cacheable     = 0x00000000LL, ///< Cache is enabled
    Noncacheable  = 0x00000010LL, ///< Caching is off
    Referenced    = 0x00000020LL, ///< Page was referenced
    Dirty         = 0x00000040LL, ///< Page was modified
    Pse_bit       = 0x00000080LL, ///< Indicates a super page
    Cpu_global    = 0x00000100LL, ///< pinned in the TLB
    L4_global     = 0x00000200LL, ///< pinned in the TLB
    XD            = 0x8000000000000000ULL,
    ATTRIBS_MASK  = 0x8000000000000006ULL
  };
};

class Pte_ptr
{
public:
  Pte_ptr(void *pte, unsigned char level) : pte((Mword*)pte), level(level) {}
  Pte_ptr() = default;

  typedef Mword Entry;
  Entry *pte;
  unsigned char level;
};

typedef Ptab::Tupel< Ptab::Traits<Unsigned64, 39, 9, false>,
                     Ptab::Traits<Unsigned64, 30, 9, true>,
                     Ptab::Traits<Unsigned64,  21, 9, true>,
                     Ptab::Traits<Unsigned64,  12, 9, true> >::List Ptab_traits;

typedef Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List Ptab_traits_vpn;
typedef Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift> Ptab_va_vpn;
