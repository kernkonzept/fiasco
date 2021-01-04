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
    // initializing frame is not needed but GCC complains with
    // "may be used uninitialized" in map_util-objs map ...
    // triggering a warning in Kobject_mapdb::grant
    //
    // As common perception seems to be that compiling without warnings is
    // more important than runtime we always initialize frame to 0 in the
    // constructor, even if this would probably cause more harm than good if
    // used with a 0 pointer as there could be a page mapped at 0 as well
    Kobject_mappable* frame = 0;
    Mapping *m = 0;

    inline size_t size() const;

    void clear(bool = false)
    {
      frame->_lock.clear();
      frame = nullptr;
    }

    void might_clear(bool = false)
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
Kobject_mapdb::lookup(Space *, Vaddr va, Phys_addr obj,
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

PUBLIC inline static
int
Kobject_mapdb::lookup_src_dst(Space *sspc, Phys_addr sobj, Vaddr sva, Obj_space::V_pfc,
                              Space *dspc, Phys_addr dobj, Vaddr dva, Obj_space::V_pfc,
                              Frame *sframe, Frame *dframe)
{
  Kobject_mappable *srn = sobj->map_root();
  srn->_lock.lock();

  if (sva._c->obj() != sobj)
    {
      srn->_lock.clear();
      return -1;
    }

  Kobject_mappable *drn = dobj->map_root();
  bool same_obj = drn == srn;

  if (!same_obj)
    drn->_lock.lock();

  if (dva._c->obj() != dobj)
    {
      if (!same_obj)
        drn->_lock.clear();
      srn->_lock.clear();
      return -1;
    }

  sframe->m = sva._c;
  sframe->frame = srn;

  dframe->m = dva._c;
  dframe->frame = drn;

  return (sspc == dspc && sva == dva) ? 2 : 1;
}

PUBLIC static inline
bool
Kobject_mapdb::valid_address(Phys_addr obj)
{ return obj; }


// FAKE
PUBLIC static inline 
Page_number
Kobject_mapdb::vaddr(Frame const &)
{ return Page_number(0); }

PUBLIC inline static
Kobject_mapdb::Mapping *
Kobject_mapdb::insert(Frame const &, Space *,
                      Vaddr va, Obj_space::Phys_addr o, Obj_space::V_pfc size)
{
  (void)size;
  (void)o;
  assert (size == Obj_space::V_pfc(1));

  Mapping *m = va._c;
  Kobject_mappable *rn = o->map_root();
  //LOG_MSG_3VAL(current(), "ins", o->dbg_id(), (Mword)m, (Mword)va._a.value());
  rn->_root.add(m);

  Obj::Entry *e = static_cast<Obj::Entry*>(m);
  if (e->ref_cnt()) // counted
    ++rn->_cnt;

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

  if (se->ref_cnt() && !re->ref_cnt())
    --f.frame->_cnt;

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

  bool flush = false;

  if (mask.do_delete() && f.m->delete_rights())
    flush = true;
  else
    {
      Obj::Entry *e = static_cast<Obj::Entry*>(f.m);
      if (e->ref_cnt()) // counted
	flush = --f.frame->_cnt <= 0;

      if (!flush)
	Mapping::List::remove(f.m);
    }

  if (flush)
    {
      for (auto const &&i: f.frame->_root)
        {
          Obj::Entry *e = static_cast<Obj::Entry*>(i);
          if (e->ref_cnt()) // counted
            --f.frame->_cnt;
          e->invalidate();
        }
      f.frame->_root.clear();
    }

} // flush()


