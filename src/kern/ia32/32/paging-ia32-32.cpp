INTERFACE[ia32,ux]:

#include <cassert>
#include "types.h"
#include "config.h"
#include "ptab_base.h"

class Pt_entry
{
public:
  enum
  {
    Super_level   = 0,
    Valid         = 0x00000001, ///< Valid
    Writable      = 0x00000002, ///< Writable
    User          = 0x00000004, ///< User accessible
    Write_through = 0x00000008, ///< Write through
    Cacheable     = 0x00000000, ///< Cache is enabled
    Noncacheable  = 0x00000010, ///< Caching is off
    Referenced    = 0x00000020, ///< Page was referenced
    Dirty         = 0x00000040, ///< Page was modified
    Pse_bit       = 0x00000080, ///< Indicates a super page
    Cpu_global    = 0x00000100, ///< pinned in the TLB
    L4_global     = 0x00000200, ///< pinned in the TLB
    XD            = 0,
    ATTRIBS_MASK  = 0x06,
  };
};

class Pte_ptr
{
public:
  Pte_ptr(void *pte, unsigned char level) : pte((Mword*)pte), level(level) {}
  Pte_ptr() = default;

  typedef Mword Entry;
  Entry *pte;
  Entry entry() const { return *pte; }
  unsigned char level;
};



typedef Ptab::List< Ptab::Traits<Unsigned32, 22, 10, true, false>,
                    Ptab::Traits<Unsigned32, 12, 10, true> > Ptab_traits;

typedef Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List Ptab_traits_vpn;
typedef Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift> Ptab_va_vpn;
typedef Pdir_t<Pte_ptr, Ptab_traits_vpn, Ptab_va_vpn> Pdir;
class Kpdir : public Pdir {};

