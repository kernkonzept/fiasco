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
                     Ptab::Traits< Unsigned64, 12, 9, true> >::List Ptab_traits;

typedef Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List Ptab_traits_vpn;
typedef Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift> Ptab_va_vpn;
typedef Pdir_t<Pte_ptr, Ptab_traits_vpn, Ptab_va_vpn> Pdir;
class Kpdir : public Pdir_t<K_pte_ptr, Ptab_traits_vpn, Ptab_va_vpn> {};
