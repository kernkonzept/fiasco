INTERFACE [arm && arm_lpae]:

EXTENSION class K_pte_ptr
{
public:
  enum
  {
    Super_level    = 2,
    Max_level      = 3,
  };
};

typedef Ptab::Tupel< Ptab::Traits< Unsigned64, 39, 9, false>,
                     Ptab::Traits< Unsigned64, 30, 9, true>,
                     Ptab::Traits< Unsigned64, 21, 9, true>,
                     Ptab::Traits< Unsigned64, 12, 9, true> >::List K_ptab_traits;

typedef Ptab::Shift<K_ptab_traits, Virt_addr::Shift>::List K_ptab_traits_vpn;
typedef Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift> Ptab_va_vpn;
class Kpdir : public Pdir_t<K_pte_ptr, K_ptab_traits_vpn, Ptab_va_vpn> {};

//---------------------------------------------------------------------------
INTERFACE [arm && cpu_virt]:

/* we currently use only 3-levels for stage 2 paging with a fixed IPA siaze of 40bits */
typedef Ptab::Tupel< Ptab::Traits< Unsigned64, 30, 10, true>,
                     Ptab::Traits< Unsigned64, 21, 9, true>,
                     Ptab::Traits< Unsigned64, 12, 9, true> >::List Ptab_traits;

typedef Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List Ptab_traits_vpn;

class Pte_ptr : public Pte_ptr_t<Pte_ptr>
{
public:
  enum
  {
    Super_level = 1,
    Max_level   = 2,
  };
  Pte_ptr() = default;
  Pte_ptr(void *p, unsigned char level) : Pte_ptr_t(p, level) {}

  unsigned char page_order() const
  { return Ptab::Level<Ptab_traits_vpn>::shift(level) + Ptab_traits_vpn::Head::Base_shift; }
};

typedef Pdir_t<Pte_ptr, Ptab_traits_vpn, Ptab_va_vpn> Pdir;

//---------------------------------------------------------------------------
INTERFACE [arm && !cpu_virt]:

typedef K_pte_ptr Pte_ptr;
typedef Pdir_t<Pte_ptr, K_ptab_traits_vpn, Ptab_va_vpn> Pdir;

