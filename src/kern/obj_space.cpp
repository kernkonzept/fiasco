INTERFACE:

#include "obj_space_types.h"
#include "config.h"
#include "l4_types.h"
#include "template_math.h"

class Kobject;
class Space;


template< typename SPACE >
class Generic_obj_space
{
  friend class Jdb_obj_space;
  friend class Jdb_tcb;

public:
  static char const * const name;

  typedef Obj::Attr Attr;
  typedef Obj::Capability Capability;
  typedef Obj::Entry Entry;
  typedef Kobject *Reap_list;
  typedef Kobject_iface *Phys_addr;

  typedef Obj::Cap_addr V_pfn;
  typedef Cap_diff V_pfc;
  typedef Order Page_order;


  enum
  {
    Need_insert_tlb_flush = 0,
    Need_xcpu_tlb_flush = 0,
    Map_page_size = 1,
    Page_shift = 0,
    Map_max_address = 1UL << 20, /* 20bit obj index */
    Whole_space = 20,
    Identity_map = 0,
  };

  typedef Obj::Insert_result Status;
  static Status const Insert_ok = Obj::Insert_ok;
  static Status const Insert_warn_exists = Obj::Insert_warn_exists;
  static Status const Insert_warn_attrib_upgrade = Obj::Insert_warn_attrib_upgrade;
  static Status const Insert_err_nomem = Obj::Insert_err_nomem;
  static Status const Insert_err_exists = Obj::Insert_err_exists;

  struct Fit_size
  {
    Order operator () (Order s) const
    {
      return s >= Order(Obj::Caps_per_page_ld2)
             ? Order(Obj::Caps_per_page_ld2)
             : Order(0);
    }
  };

  Fit_size fitting_sizes() const { return Fit_size(); }

  static Phys_addr page_address(Phys_addr o, Order) { return o; }
  static Phys_addr subpage_address(Phys_addr addr, V_pfc) { return addr; }
  static V_pfn page_address(V_pfn addr, Order) { return addr; }
  static V_pfc subpage_offset(V_pfn addr, Order o) { return cxx::get_lsb(addr, o); }

  static Phys_addr to_pfn(Phys_addr p) { return p; }
  static V_pfn to_pfn(V_pfn p) { return p; }
  static V_pfc to_pcnt(Order s) { return V_pfc(1) << s; }

  static V_pfc to_size(Page_order p)
  { return V_pfc(1) << p; }

  FIASCO_SPACE_VIRTUAL
  bool v_lookup(V_pfn const &virt, Phys_addr *phys = 0,
                Page_order *size = 0, Attr *attribs = 0);

  FIASCO_SPACE_VIRTUAL
  L4_fpage::Rights v_delete(V_pfn virt, Order size,
                            L4_fpage::Rights page_attribs);

  FIASCO_SPACE_VIRTUAL
  Status v_insert(Phys_addr phys, V_pfn const &virt, Order size,
                  Attr page_attribs);

  FIASCO_SPACE_VIRTUAL
  Capability lookup(Cap_index virt);

  FIASCO_SPACE_VIRTUAL
  V_pfn obj_map_max_address() const;

  FIASCO_SPACE_VIRTUAL
  void caps_free();

  Kobject_iface *lookup_local(Cap_index virt, L4_fpage::Rights *rights = 0);

  inline V_pfn map_max_address() const
  { return obj_map_max_address(); }
};

template< typename SPACE >
char const * const Generic_obj_space<SPACE>::name = "Obj_space";


// ------------------------------------------------------------------------------
INTERFACE [debug]:

EXTENSION class Generic_obj_space
{
public:
  FIASCO_SPACE_VIRTUAL Entry *jdb_lookup_cap(Cap_index index);
};

// -------------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC template< typename SPACE >
static inline
Mword
Generic_obj_space<SPACE>::xlate_flush(L4_fpage::Rights rights)
{ return L4_fpage::Rights::val(rights); }

PUBLIC template< typename SPACE >
static inline
bool
Generic_obj_space<SPACE>::is_full_flush(L4_fpage::Rights rights)
{ return (bool)(rights & L4_fpage::Rights::R()); }

PUBLIC template< typename SPACE >
static inline
L4_fpage::Rights
Generic_obj_space<SPACE>::xlate_flush_result(Mword /*attribs*/)
{ return L4_fpage::Rights(0); }

PUBLIC template< typename SPACE >
inline
Generic_obj_space<SPACE>::~Generic_obj_space()
{
  this->caps_free();
}

IMPLEMENT template< typename SPACE > inline
void FIASCO_FLATTEN
Generic_obj_space<SPACE>::caps_free()
{ Base::caps_free(); }

PUBLIC template< typename SPACE >
inline
bool
Generic_obj_space<SPACE>::v_fabricate(V_pfn const &address,
                                      Phys_addr *phys, Page_order *size,
                                      Attr* attribs = 0)
{
  return this->v_lookup(address, phys, size, attribs);
}


PUBLIC template< typename SPACE >
inline static
void
Generic_obj_space<SPACE>::tlb_flush_spaces(bool, Generic_obj_space<SPACE> *,
                                           Generic_obj_space<SPACE> *)
{}

PUBLIC template< typename SPACE >
inline static
void
Generic_obj_space<SPACE>::tlb_flush()
{}

PUBLIC template< typename SPACE >
inline static
typename Generic_obj_space<SPACE>::V_pfn
Generic_obj_space<SPACE>::canonize(V_pfn v)
{ return v; }

