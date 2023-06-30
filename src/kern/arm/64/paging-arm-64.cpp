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

/* 3-levels for stage 2 paging with a maximum IPA size of 40bits */
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
    Min_pa_range = 0, // Can be used for everything smaller than 40 bits.
    Max_pa_range = 2, // 40 bits PA/IPA size (encoded as VTCR_EL2.PS)
    Vtcr_sl0 = 1,     // 3 level page table
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && cpu_virt && arm_pt_48]:

/* 4-levels for stage 2 paging with a maximum IPA size of 48bits */
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
    Min_pa_range = 4, // 4-level stage2 PTs need at least 44 (I)PA bits!
    Max_pa_range = 5, // 48 bits PA/IPA size (encoded as VTCR_EL2.PS)
    Vtcr_sl0 = 2,     // 4 level page table
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

  static unsigned inline ipa_bits(unsigned pa_range)
  {
    static char const pa_range_bits[] = { 32, 36, 40, 42, 44, 48, 52 };
    if (pa_range > Max_pa_range)
      pa_range = Max_pa_range;

    return pa_range_bits[pa_range];
  }

  static unsigned inline vtcr_bits(unsigned pa_range)
  {
    if (pa_range > Max_pa_range)
      pa_range = Max_pa_range;

    unsigned pa_bits = ipa_bits(pa_range);

    return (Vtcr_sl0            <<  6)  // SL0
            | (pa_range         << 16)  // PS
            | ((64U - pa_bits)  <<  0)  // T0SZ
            | ((Mem_unit::Asid_bits == 16) << 19); // VS
  }
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
                                // (AS)   ASID Size
                 | ((Mem_unit::Asid_bits == 16 ? 1UL : 0UL) << 36)
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
