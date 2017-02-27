INTERFACE [arm && arm_lpae]:

EXTENSION class K_pte_ptr
{
public:
  enum
  {
    Super_level    = 1,
    Max_level      = 2,
  };
};

EXTENSION class Page
{
public:
  enum
  {
    Ttbcr_bits =   (1 << 31) // EAE
                 | (Tcr_attribs << 8),
  };
};

typedef Ptab::Tupel< Ptab::Traits< Unsigned64, 30, 2, true>,
                     Ptab::Traits< Unsigned64, 21, 9, true>,
                     Ptab::Traits< Unsigned64, 12, 9, true> >::List Ptab_traits;

typedef Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List Ptab_traits_vpn;
typedef Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift> Ptab_va_vpn;

//---------------------------------------------------------------------------
INTERFACE [arm]:

/** for ARM 32bit we use identical page-table layouts for kernel and user */
class Kpdir : public Pdir_t<K_pte_ptr, Ptab_traits_vpn, Ptab_va_vpn> {};

//---------------------------------------------------------------------------
INTERFACE [arm && cpu_virt]:

/** when using stage 2 paging the attributes in the page-tables differ */
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

/** without stage 2 paging kernel and user use the same page table. */
typedef K_pte_ptr Pte_ptr;
typedef Pdir_t<Pte_ptr, Ptab_traits_vpn, Ptab_va_vpn> Pdir;

