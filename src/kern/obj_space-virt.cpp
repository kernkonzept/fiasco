INTERFACE:

#include "mem.h"
#include "mem_space.h"
#include "ram_quota.h"
#include "obj_space_virt_util.h"

EXTENSION class Generic_obj_space : Obj_space_virt<Generic_obj_space<SPACE> >
{
  typedef Obj_space_virt<Generic_obj_space<SPACE> > Base;
};

IMPLEMENTATION:

PUBLIC  template< typename SPACE > static
inline
Ram_quota *
Generic_obj_space<SPACE>::ram_quota(Base const *base)
{ return static_cast<SPACE const *>(base)->ram_quota(); }


PUBLIC template< typename SPACE > static inline
Mem_space *
Generic_obj_space<SPACE>::mem_space(Base *base)
{ return static_cast<SPACE*>(base); }

//
// Utilities for map<Generic_obj_space> and unmap<Generic_obj_space>
//

IMPLEMENT  template< typename SPACE >
inline
bool FIASCO_FLATTEN
Generic_obj_space<SPACE>::v_lookup(V_pfn const &virt, Phys_addr *phys,
                                   Page_order *size, Attr *attribs)
{
  return Base::v_lookup(virt, phys, size, attribs);
}

IMPLEMENT template< typename SPACE >
inline
typename Generic_obj_space<SPACE>::Capability FIASCO_FLATTEN
Generic_obj_space<SPACE>::lookup(Cap_index virt)
{ return Base::lookup(virt); }

IMPLEMENT template< typename SPACE >
inline
Kobject_iface * FIASCO_FLATTEN
Generic_obj_space<SPACE>::lookup_local(Cap_index virt, L4_fpage::Rights *rights = 0)
{ return Base::lookup_local(virt, rights); }


IMPLEMENT template< typename SPACE >
inline
L4_fpage::Rights FIASCO_FLATTEN
Generic_obj_space<SPACE>::v_delete(V_pfn virt, Order size,
                                   L4_fpage::Rights page_attribs)
{ return  Base::v_delete(virt, size, page_attribs); }

IMPLEMENT  template< typename SPACE >
inline
typename Generic_obj_space<SPACE>::Status FIASCO_FLATTEN
Generic_obj_space<SPACE>::v_insert(Phys_addr phys, V_pfn const &virt, Order size,
                                   Attr page_attribs)
{ return (Status)Base::v_insert(phys, virt, size, page_attribs); }


IMPLEMENT template< typename SPACE >
inline
typename Generic_obj_space<SPACE>::V_pfn FIASCO_FLATTEN
Generic_obj_space<SPACE>::obj_map_max_address() const
{ return Base::obj_map_max_address(); }

// ------------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC template< typename SPACE > static inline
SPACE *
Generic_obj_space<SPACE>::get_space(Base *base)
{ return static_cast<SPACE*>(base); }

IMPLEMENT template< typename SPACE > inline
typename Generic_obj_space<SPACE>::Entry *
Generic_obj_space<SPACE>::jdb_lookup_cap(Cap_index index)
{ return Base::jdb_lookup_cap(index); }

// ------------------------------------------------------------------------------
IMPLEMENTATION [!debug]:

PUBLIC template< typename SPACE > static inline
SPACE *
Generic_obj_space<SPACE>::get_space(Base *)
{ return 0; }
