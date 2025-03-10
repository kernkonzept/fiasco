INTERFACE:

#include "types.h"
#include "space.h"
#include "kobject.h"

class Kobject;

/** A mapping database.
 */
class Kobject_mapdb
{
public:
  // TYPES

  typedef Obj_space::Phys_addr Phys_addr;
  typedef Obj_space::V_pfn Vaddr;
  typedef Obj::Mapping Mapping;
  typedef int Order;

  class Iterator;
  class Frame
  {
  public:
    // Initializing these members is not required because both are initialized
    // before they are used. However, just to be on the safe side, perform the
    // initialization anyway, as the code flow is not so obvious.
    Kobject_mappable *frame = nullptr;
    Mapping *m = nullptr;

    void clear()
    {
      frame->_lock.clear();
      frame = nullptr;
    }

    void might_clear()
    {
      if (frame)
        clear();
    }

    bool same_lock(Frame const &o) const
    {
      return frame == o.frame;
    }
  };

  template< typename F >
  static void foreach_mapping(Frame const &, Obj_space::V_pfn, Obj_space::V_pfn, F)
  {}
};


// ---------------------------------------------------------------------------
IMPLEMENTATION:

#include <cassert>

#include "config.h"
#include "globals.h"
#include "std_macros.h"

PUBLIC inline static
bool
Kobject_mapdb::lookup(Space const *, Vaddr va, Phys_addr obj,
                      Frame *out)
{
  Kobject_mappable *rn = obj->map_root(); 
  rn->_lock.lock();
  if (va._c->obj() == obj)
    {
      out->m = va._c;
      out->frame = rn;
      return true;
    }

  rn->_lock.clear();
  return false;
}

PRIVATE static inline
bool
Kobject_mapdb::lock_cap_pair(Phys_addr obj1, Vaddr va1, Frame *frame1,
                             Phys_addr obj2, Vaddr va2, Frame *frame2)
{
  Kobject_mappable *rn1 = obj1->map_root();
  rn1->_lock.lock();

  if (va1._c->obj() != obj1)
    {
      rn1->_lock.clear();
      return false;
    }

  Kobject_mappable *rn2 = obj2->map_root();
  bool same_obj = rn2 == rn1;

  if (!same_obj)
    rn2->_lock.lock();

  if (va2._c->obj() != obj2)
    {
      if (!same_obj)
        rn2->_lock.clear();
      rn1->_lock.clear();
      return false;
    }

  frame1->m = va1._c;
  frame1->frame = rn1;

  frame2->m = va2._c;
  frame2->frame = rn2;

  return true;
}

PUBLIC static inline NEEDS[Kobject_mapdb::lock_cap_pair]
int
Kobject_mapdb::lookup_src_dst(Space const *sspc, Phys_addr sobj, Vaddr sva,
                              Space const *dspc, Phys_addr dobj, Vaddr dva,
                              Frame *sframe, Frame *dframe)
{
  // Lock order is determined by object address to have a reliable lock
  // ordering. A second thread could call concurrently with swapped
  // sobj/dobj parameters.
  if (!(sobj < dobj ? lock_cap_pair(sobj, sva, sframe, dobj, dva, dframe)
                    : lock_cap_pair(dobj, dva, dframe, sobj, sva, sframe)))
    return -1;

  return (sspc == dspc && sva == dva) ? 2 : 1;
}

PUBLIC static inline
bool
Kobject_mapdb::valid_address(Phys_addr obj)
{ return obj; }


PUBLIC inline static
Kobject_mapdb::Mapping *
Kobject_mapdb::insert(Frame const &, Space *,
                      Vaddr va, Obj_space::Phys_addr o, [[maybe_unused]] Obj_space::V_pfc size)
{
  assert (size == Obj_space::V_pfc(1));

  Mapping *m = va._c;
  Kobject_mappable *rn = o->map_root();
  //LOG_MSG_3VAL(current(), "ins", o->dbg_id(), (Mword)m, (Mword)va._a.value());
  rn->_root.add(m);

  Obj::Entry *e = static_cast<Obj::Entry*>(m);
  if (e->is_ref_counted())
    {
      // No overflow check required. The counter has type Smword and can count
      // half of the addresses in the virtual address space. A capability has a
      // size of at least one Mword but in fact a capability mapping occupies
      // more memory than a single Mword.
      static_assert(sizeof(rn->_cnt) >= sizeof(void*),
                    "Wrong type for reference counter");
      ++rn->_cnt;
    }

  return m;
}

PUBLIC inline static
bool
Kobject_mapdb::grant(Frame &f, Space *, Vaddr va)
{
  Obj::Entry *re = va._c;
  Obj::Entry *se = static_cast<Obj::Entry*>(f.m);
  //LOG_MSG_3VAL(current(), "gra", f.frame->dbg_id(), (Mword)sm, (Mword)va._a.value());

  // replace the source cap with the destination cap in the list
  Mapping::List::replace(se, re);

  if (se->is_ref_counted() && !re->is_ref_counted())
    if (--f.frame->_cnt <= 0)
      f.frame->invalidate_mappings();

  return true;
}

PUBLIC static inline
void
Kobject_mapdb::flush(Frame const &f, L4_map_mask mask,
                     Obj_space::V_pfn, Obj_space::V_pfn)
{
  //LOG_MSG_3VAL(current(), "unm", f.frame->dbg_id(), (Mword)m, 0);
  if (!mask.self_unmap())
    return;

  if (mask.do_delete() && f.m->delete_rights())
    {
      f.frame->invalidate_mappings();
      return;
    }

  if (!static_cast<Obj::Entry*>(f.m)->is_ref_counted())
    {
      Mapping::List::remove(f.m);
      return;
    }

  if (f.frame->_cnt <= 1)
    {
      f.frame->invalidate_mappings();
      return;
    }

  --f.frame->_cnt;
  Mapping::List::remove(f.m);

} // flush()
