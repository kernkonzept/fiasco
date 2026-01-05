INTERFACE [arm && mmu && arm_lpae]:

EXTENSION class K_pte_ptr
{
public:
  static constexpr unsigned super_level() { return 2; }
  static constexpr unsigned max_level()   { return 3; }
};

typedef Ptab::Tupel< Ptab::Traits< Unsigned64, 39, 9, false>,
                     Ptab::Traits< Unsigned64, 30, 9, true>,
                     Ptab::Traits< Unsigned64, 21, 9, true>,
                     Ptab::Traits< Unsigned64, 12, 9, true> >::List K_ptab_traits;

typedef Ptab::Shift<K_ptab_traits, Virt_addr::Shift>::List K_ptab_traits_vpn;
typedef Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift> Ptab_va_vpn;
class Kpdir : public Pdir_t<K_pte_ptr, K_ptab_traits_vpn, Ptab_va_vpn> {};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && cpu_virt]:

/* 3-levels for stage 2 paging with a maximum IPA size of 40bits */
typedef Ptab::Tupel< Ptab::Traits< Unsigned64, 30, 10, true>,
                     Ptab::Traits< Unsigned64, 21, 9, true>,
                     Ptab::Traits< Unsigned64, 12, 9, true> >::List Ptab_traits_3lvl;

typedef Ptab::Shift<Ptab_traits_3lvl, Virt_addr::Shift>::List Ptab_traits_vpn_3lvl;

/* 4-levels for stage 2 paging with a maximum IPA size of 48bits */
typedef Ptab::Tupel< Ptab::Traits< Unsigned64, 39, 9, false>,
                     Ptab::Traits< Unsigned64, 30, 9, true>,
                     Ptab::Traits< Unsigned64, 21, 9, true>,
                     Ptab::Traits< Unsigned64, 12, 9, true> >::List Ptab_traits_4lvl;

typedef Ptab::Shift<Ptab_traits_4lvl, Virt_addr::Shift>::List Ptab_traits_vpn_4lvl;

class Pte_ptr : public Pte_ptr_t<Pte_ptr>
{
public:
  static unsigned super_level() { return Cpubits::Pt4() ? 2 : 1; }
  static unsigned max_level()   { return Cpubits::Pt4() ? 3 : 2; }

  unsigned page_level() const { return level; }
  Pte_ptr() = default;
  Pte_ptr(void *p, unsigned char level) : Pte_ptr_t(p, level) {}
};

typedef Pdir_t<Pte_ptr, Ptab_traits_vpn_3lvl, Ptab_va_vpn> Pdir_3lvl;
typedef Pdir_t<Pte_ptr, Ptab_traits_vpn_4lvl, Ptab_va_vpn> Pdir_4lvl;

PUBLIC inline ALWAYS_INLINE
unsigned char
Pte_ptr::page_order() const
{
  return Cpubits::Pt4() ? Pdir_4lvl::page_order_for_level(level)
                        : Pdir_3lvl::page_order_for_level(level);
}

/* Wrapper for Pdir_3lvl and Pdir_4lvl */
class Pdir
{
private:
  union
  {
    Pdir_3lvl _3lvl;
    Pdir_4lvl _4lvl;
  };

public:
  typedef Addr::Addr<0> Va;
  typedef Addr::Addr<0>::Diff_type Vs;

  typedef ::Pte_ptr Pte_ptr;

  Address virt_to_phys(Address virt) const
  {
    return Cpubits::Pt4() ? _4lvl.virt_to_phys(virt)
                          : _3lvl.virt_to_phys(virt);
  }

  void clear(bool force_write_back)
  {
    // _3lvl or _4lvl does not matter
    _3lvl.clear(force_write_back);
  }

  static unsigned depth()
  {
    return Cpubits::Pt4() ? unsigned{Pdir_4lvl::Depth}
                          : unsigned{Pdir_3lvl::Depth};
  }

  static unsigned super_level() { return Pte_ptr::super_level(); }

  static Address max_addr()
  {
    return Cpubits::Pt4() ? Address{Pdir_4lvl::Max_addr}
                          : Address{Pdir_3lvl::Max_addr};
  }

  static unsigned page_order_for_level(unsigned level)
  {
    return Cpubits::Pt4() ? Pdir_4lvl::page_order_for_level(level)
                          : Pdir_3lvl::page_order_for_level(level);
  }

  template< typename _Alloc >
  Pte_ptr walk(Va va, unsigned level, bool force_write_back, _Alloc &&alloc)
  {
    return Cpubits::Pt4() ? _4lvl.walk(va, level, force_write_back, alloc)
                          : _3lvl.walk(va, level, force_write_back, alloc);
  }

  Pte_ptr walk(Va virt)
  {
    return Cpubits::Pt4() ? _4lvl.walk(virt) : _3lvl.walk(virt);
  }

  template< typename _Alloc >
  void destroy(Va start, Va end, unsigned start_level, unsigned end_level, _Alloc &&alloc)
  {
    if (Cpubits::Pt4())
      _4lvl.destroy(start, end, start_level, end_level, alloc);
    else
      _3lvl.destroy(start, end, start_level, end_level, alloc);
  }


  // Used in JDB
  static unsigned lsb_for_level(unsigned level)
  {
    return Cpubits::Pt4() ? Pdir_4lvl::lsb_for_level(level)
                          : Pdir_3lvl::lsb_for_level(level);
  }

  struct Levels
  {
    static unsigned long length(unsigned level)
    {
      return Cpubits::Pt4() ? Pdir_4lvl::Levels::length(level)
                            : Pdir_3lvl::Levels::length(level);
    }

    static unsigned long entry_size(unsigned level)
    {
      return Cpubits::Pt4() ? Pdir_4lvl::Levels::entry_size(level)
                            : Pdir_3lvl::Levels::entry_size(level);
    }
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && !cpu_virt]:

typedef K_pte_ptr Pte_ptr;
typedef Pdir_t<Pte_ptr, K_ptab_traits_vpn, Ptab_va_vpn> Pdir;
