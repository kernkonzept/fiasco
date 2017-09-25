INTERFACE:

#include "mem.h"
#include "mem_space.h"
#include "ram_quota.h"
#include "obj_space_types.h"

template<typename SPACE>
class Obj_space_virt
{
public:
  typedef Obj::Attr Attr;
  typedef Obj::Capability Capability;
  typedef Obj::Entry Entry;
  typedef Kobject_iface *Phys_addr;

  typedef Obj::Cap_addr V_pfn;
  typedef Cap_diff V_pfc;
  typedef Order Page_order;

  bool v_lookup(V_pfn const &virt, Phys_addr *phys,
                Page_order *size, Attr *attribs);

  L4_fpage::Rights v_delete(V_pfn virt, Order size,
                            L4_fpage::Rights page_attribs);
  Obj::Insert_result v_insert(Phys_addr phys, V_pfn const &virt, Order size,
                              Attr page_attribs);

  Capability lookup(Cap_index virt);

private:
  enum
  {
    // do not use the virtually mapped cap table in
    // v_lookup and v_insert, because the map logic needs the kernel
    // address for link pointers in the map-nodes and these addresses must
    // be valid in all address spaces.
    Optimize_local = 0,

    Whole_space = 20,
    Map_max_address = 1UL << 20, /* 20bit obj index */
  };
};

IMPLEMENTATION:

#include <cstring>
#include <cassert>

#include "atomic.h"
#include "config.h"
#include "cpu.h"
#include "kmem_alloc.h"
#include "mem_layout.h"

PRIVATE  template< typename SPACE >
static inline NEEDS["mem_layout.h"]
typename Obj_space_virt<SPACE>::Entry *
Obj_space_virt<SPACE>::cap_virt(Cap_index index)
{ return reinterpret_cast<Entry*>(Mem_layout::Caps_start) + cxx::int_value<Cap_index>(index); }



PRIVATE template< typename SPACE >
inline NEEDS["mem_space.h", "mem_layout.h", Obj_space_virt::cap_virt]
typename Obj_space_virt<SPACE>::Entry *
Obj_space_virt<SPACE>::get_cap(Cap_index index)
{
  Mem_space *ms = SPACE::mem_space(this);

  Address phys = Address(ms->virt_to_phys((Address)cap_virt(index)));
  if (EXPECT_FALSE(phys == ~0UL))
    return 0;

  return reinterpret_cast<Entry*>(Mem_layout::phys_to_pmem(phys));
}

PRIVATE  template< typename SPACE >
/*inline NEEDS["kmem_alloc.h", <cstring>, "ram_quota.h",
                     Obj_space_virt::cap_virt]*/
typename Obj_space_virt<SPACE>::Entry *
Obj_space_virt<SPACE>::caps_alloc(Cap_index virt)
{
  Address cv = (Address)cap_virt(virt);
  void *mem = Kmem_alloc::allocator()->q_unaligned_alloc(SPACE::ram_quota(this), Config::PAGE_SIZE);

  if (!mem)
    return 0;

  Obj::add_cap_page_dbg_info(mem, SPACE::get_space(this), cxx::int_value<Cap_index>(virt));

  Mem::memset_mwords(mem, 0, Config::PAGE_SIZE / sizeof(Mword));

  Mem_space::Status s;
  s = SPACE::mem_space(this)->v_insert(
      Mem_space::Phys_addr(Mem_space::kernel_space()->virt_to_phys((Address)mem)),
      cxx::mask_lsb(Virt_addr(cv), Mem_space::Page_order(Config::PAGE_SHIFT)),
      Mem_space::Page_order(Config::PAGE_SHIFT),
      Mem_space::Attr(L4_fpage::Rights::RW()));
      //| Mem_space::Page_referenced | Mem_space::Page_dirty);

  switch (s)
    {
    case Mem_space::Insert_ok:
      break;
    case Mem_space::Insert_warn_exists:
    case Mem_space::Insert_warn_attrib_upgrade:
      assert (false);
      break;
    case Mem_space::Insert_err_exists:
    case Mem_space::Insert_err_nomem:
      Kmem_alloc::allocator()->q_unaligned_free(SPACE::ram_quota(this),
          Config::PAGE_SIZE, mem);
      return 0;
    };

  unsigned long cap = (cv & (Config::PAGE_SIZE - 1)) | (unsigned long)mem;

  return reinterpret_cast<Entry*>(cap);
}

PROTECTED template< typename SPACE >
void
Obj_space_virt<SPACE>::caps_free()
{
  Mem_space *ms = SPACE::mem_space(this);
  if (EXPECT_FALSE(!ms || !ms->dir()))
    return;

  Kmem_alloc *a = Kmem_alloc::allocator();
  for (Cap_index i = Cap_index(0); i < obj_map_max_address();
       i += Cap_diff(Obj::Caps_per_page))
    {
      Entry *c = get_cap(i);
      if (!c)
	continue;

      Address cp = Address(ms->virt_to_phys(Address(c)));
      assert (cp != ~0UL);
      void *cv = (void*)Mem_layout::phys_to_pmem(cp);
      Obj::remove_cap_page_dbg_info(cv);

      a->q_unaligned_free(SPACE::ram_quota(this), Config::PAGE_SIZE, cv);
    }
  ms->dir()->destroy(Virt_addr(Mem_layout::Caps_start),
                     Virt_addr(Mem_layout::Caps_end-1),
                     Pdir::Super_level,
                     Pdir::Depth,
                     Kmem_alloc::q_allocator(SPACE::ram_quota(this)));
}

//
// Utilities for map<Obj_space_virt> and unmap<Obj_space_virt>
//

IMPLEMENT  template< typename SPACE >
inline  NEEDS[Obj_space_virt::cap_virt, Obj_space_virt::get_cap]
bool FIASCO_FLATTEN
Obj_space_virt<SPACE>::v_lookup(V_pfn const &virt, Phys_addr *phys,
                                   Page_order *size, Attr *attribs)
{
  if (size) *size = Order(0);
  Entry *cap;

  if (Optimize_local
      && SPACE::mem_space(this) == Mem_space::current_mem_space(current_cpu()))
    cap = cap_virt(virt);
  else
    cap = get_cap(virt);

  if (EXPECT_FALSE(!cap))
    {
      if (size) *size = Order(Obj::Caps_per_page_ld2);
      return false;
    }

  if (Optimize_local)
    {
      Capability c = Mem_layout::read_special_safe((Capability*)cap);

      if (phys) *phys = c.obj();
      if (c.valid() && attribs)
        *attribs = Attr(c.rights());
      return c.valid();
    }
  else
    {
      Obj::set_entry(virt, cap);
      if (phys) *phys = cap->obj();
      if (cap->valid() && attribs)
        *attribs = Attr(cap->rights());
      return cap->valid();
    }
}

IMPLEMENT template< typename SPACE >
inline NEEDS [Obj_space_virt::cap_virt, Obj_space_virt::get_cap]
typename Obj_space_virt<SPACE>::Capability FIASCO_FLATTEN
Obj_space_virt<SPACE>::lookup(Cap_index virt)
{
  Capability *c;
  virt &= Cap_index(~(~0UL << Whole_space));

  if (SPACE::mem_space(this) == Mem_space::current_mem_space(current_cpu()))
    c = reinterpret_cast<Capability*>(cap_virt(virt));
  else
    c = get_cap(virt);

  if (EXPECT_FALSE(!c))
    return Capability(0); // void

  return Mem_layout::read_special_safe(c);
}

PUBLIC template< typename SPACE >
inline NEEDS [Obj_space_virt::cap_virt]
Kobject_iface *
Obj_space_virt<SPACE>::lookup_local(Cap_index virt, L4_fpage::Rights *rights)
{
  virt &= Cap_index(~(~0UL << Whole_space));
  Capability *c = reinterpret_cast<Capability*>(cap_virt(virt));
  Capability cap = Mem_layout::read_special_safe(c);
  if (rights) *rights = L4_fpage::Rights(cap.rights());
  return cap.obj();
}


IMPLEMENT template< typename SPACE >
inline NEEDS[<cassert>, Obj_space_virt::cap_virt, Obj_space_virt::get_cap]
L4_fpage::Rights FIASCO_FLATTEN
Obj_space_virt<SPACE>::v_delete(V_pfn virt, Order size,
                                   L4_fpage::Rights page_attribs)
{
  (void)size;
  assert (size == Order(0));

  Entry *c;
  if (Optimize_local
      && SPACE::mem_space(this) == Mem_space::current_mem_space(current_cpu()))
    {
      c = cap_virt(virt);
      if (!c)
	return L4_fpage::Rights(0);

      Capability cap = Mem_layout::read_special_safe((Capability*)c);
      if (!cap.valid())
	return L4_fpage::Rights(0);
    }
  else
    c = get_cap(virt);

  if (c && c->valid())
    {
      if (page_attribs & L4_fpage::Rights::R())
        c->invalidate();
      else
        c->del_rights(page_attribs & L4_fpage::Rights::CWSD());
    }

  return L4_fpage::Rights(0);
}

IMPLEMENT  template< typename SPACE >
inline NEEDS[Obj_space_virt::cap_virt, Obj_space_virt::caps_alloc,
             Obj_space_virt::get_cap, <cassert>]
typename Obj::Insert_result FIASCO_FLATTEN
Obj_space_virt<SPACE>::v_insert(Phys_addr phys, V_pfn const &virt, Order size,
                                Attr page_attribs)
{
  (void)size;
  assert (size == Order(0));

  Entry *c;

  if (Optimize_local
      && SPACE::mem_space(this) == Mem_space::current_mem_space(current_cpu()))
    {
      c = cap_virt(virt);
      if (!c)
	return Obj::Insert_err_nomem;

      Capability cap;
      if (!Mem_layout::read_special_safe((Capability*)c, cap)
	  && !caps_alloc(virt))
	return Obj::Insert_err_nomem;
    }
  else
    {
      c = get_cap(virt);
      if (!c && !(c = caps_alloc(virt)))
	return Obj::Insert_err_nomem;
      Obj::set_entry(virt, c);
    }

  if (c->valid())
    {
      if (c->obj() == phys)
	{
	  if (EXPECT_FALSE(c->rights() == page_attribs))
	    return Obj::Insert_warn_exists;

	  c->add_rights(page_attribs);
	  return Obj::Insert_warn_attrib_upgrade;
	}
      else
	return Obj::Insert_err_exists;
    }

  c->set(phys, page_attribs);
  return Obj::Insert_ok;
}


PUBLIC  template< typename SPACE >
virtual inline
typename Obj_space_virt<SPACE>::V_pfn
Obj_space_virt<SPACE>::obj_map_max_address() const
{
  Mword r;

  r = (Mem_layout::Caps_end - Mem_layout::Caps_start) / sizeof(Entry);
  if (Map_max_address < r)
    r = Map_max_address;

  return V_pfn(r);
}

// ------------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC  template< typename SPACE >
typename Obj_space_virt<SPACE>::Entry *
Obj_space_virt<SPACE>::jdb_lookup_cap(Cap_index index)
{ return get_cap(index); }

