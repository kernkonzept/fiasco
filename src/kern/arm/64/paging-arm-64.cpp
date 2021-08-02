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
INTERFACE [arm && cpu_virt && !arm_pt_48]:

/* 3-levels for stage 2 paging with a fixed IPA size of 40bits */
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
};

typedef Pdir_t<Pte_ptr, Ptab_traits_vpn, Ptab_va_vpn> Pdir;

EXTENSION class Page
{
public:
  enum
  {
    Vtcr_bits =   (1UL  <<  6) // SL0
                | (2UL  << 16) // PS
                | (25UL <<  0) // T0SZ
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && cpu_virt && arm_pt_48]:

typedef Ptab::Tupel< Ptab::Traits< Unsigned64, 39, 9, false>,
                     Ptab::Traits< Unsigned64, 30, 9, true>,
                     Ptab::Traits< Unsigned64, 21, 9, true>,
                     Ptab::Traits< Unsigned64, 12, 9, true> >::List Ptab_traits;

typedef Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List Ptab_traits_vpn;

class Pte_ptr : public Pte_ptr_t<Pte_ptr>
{
public:
  enum
  {
    Super_level = 2,
    Max_level   = 3,
  };
  Pte_ptr() = default;
  Pte_ptr(void *p, unsigned char level) : Pte_ptr_t(p, level) {}
};

typedef Pdir_t<Pte_ptr, Ptab_traits_vpn, Ptab_va_vpn> Pdir;

EXTENSION class Page
{
public:
  enum
  {
    Vtcr_bits =   (2UL  <<  6) // SL0
                | (5UL  << 16) // PS
                | (16UL <<  0) // T0SZ
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && cpu_virt]:

EXTENSION class Page
{
public:
  enum
  {
    Ttbcr_bits =   (1UL << 31) | (1UL << 23) // RES1
                 | (Tcr_attribs <<  8) // (IRGN0)
                 | (16UL <<  0) // (T0SZ) Address space size 48bits (64 - 16)
                 | (0UL  << 14) // (TG0)  Page granularity 4kb
                 | (5UL  << 16) // (PS)   Physical address size 48bits
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && !cpu_virt]:

typedef K_pte_ptr Pte_ptr;
typedef Pdir_t<Pte_ptr, K_ptab_traits_vpn, Ptab_va_vpn> Pdir;

EXTENSION class Page
{
public:
  enum
  {
    Ttbcr_bits =   (Tcr_attribs <<  8) // (IRGN0)
                 | (Tcr_attribs << 24) // (IRGN1)
                 | (16UL <<  0) // (T0SZ) Address space size 48bits (64 - 16)
                 | (16UL << 16) // (T1SZ) Address space size 48bits (64 - 16)
                 | (0UL  << 14) // (TG0)  Page granularity 4kb
                 | (2UL  << 30) // (TG1)  Page granularity 4kb
                 | (5UL  << 32) // (IPS)  Physical address size 48bits
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

PUBLIC inline
unsigned char
Pte_ptr::page_order() const
{
  return Pdir::page_order_for_level(level);
}
