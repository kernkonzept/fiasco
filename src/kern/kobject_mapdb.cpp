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
    friend class Kobject_mapdb;
    friend class Kobject_mapdb::Iterator;
    // initializing frame is not needed but GCC complains with
    // "may be used uninitialized" in map_util-objs map ...
    // triggering a warning in Kobject_mapdb::grant
    //
    // As common perception seems to be that compiling without warnings is
    // more important than runtime we always initialize frame to 0 in the
    // constructor, even if this would probably cause more harm than good if
    // used with a 0 pointer as there could be a page mapped at 0 as well
    Kobject_mappable* frame = 0;

  public:
    inline size_t size() const;
  };

  template< typename F >
  static void foreach_mapping(Frame const &, Mapping *, Obj_space::V_pfn, Obj_space::V_pfn, F)
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
                      Mapping** out_mapping, Frame* out_lock)
{
  Kobject_mappable *rn = obj->map_root(); 
  rn->_lock.lock();
  if (va._c->obj() == obj)
    {
      *out_mapping = va._c;
      out_lock->frame = rn;
      return true;
    }

  rn->_lock.clear();
  return false;
}

PUBLIC static inline
bool
Kobject_mapdb::valid_address(Phys_addr obj)
{ return obj; }


// FAKE
PUBLIC static inline 
Page_number
Kobject_mapdb::vaddr(const Frame&, Mapping*)
{ return Page_number(0); }

PUBLIC inline
Kobject_mapdb::Mapping *
Kobject_mapdb::check_for_upgrade(Obj_space::Phys_addr,
                                 Space *, Obj_space::V_pfn,
                                 Space *, Obj_space::V_pfn,
                                 Frame *)
{
  // Hm we never or seldomly do upgrades on cap anyway
  return 0;
}

PUBLIC inline static
Kobject_mapdb::Mapping *
Kobject_mapdb::insert(const Frame&, Mapping*, Space *,
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
Kobject_mapdb::grant(const Frame &f, Mapping *sm, Space *,
                     Vaddr va)
{
  Obj::Entry *re = va._c;
  Obj::Entry *se = static_cast<Obj::Entry*>(sm);
  //LOG_MSG_3VAL(current(), "gra", f.frame->dbg_id(), (Mword)sm, (Mword)va._a.value());

  // replace the source cap with the destination cap in the list
  Mapping::List::replace(se, re);

  if (se->ref_cnt() && !re->ref_cnt())
    --f.frame->_cnt;

  return true;
}

PUBLIC inline
static void 
Kobject_mapdb::free (const Frame& f)
{
  f.frame->_lock.clear();
} // free()

PUBLIC static inline
void
Kobject_mapdb::flush(const Frame& f, Mapping *m, L4_map_mask mask,
                     Obj_space::V_pfn, Obj_space::V_pfn)
{
  //LOG_MSG_3VAL(current(), "unm", f.frame->dbg_id(), (Mword)m, 0);
  if (!mask.self_unmap())
    return;

  bool flush = false;

  if (mask.do_delete() && m->delete_rights())
    flush = true;
  else
    {
      Obj::Entry *e = static_cast<Obj::Entry*>(m);
      if (e->ref_cnt()) // counted
	flush = --f.frame->_cnt <= 0;

      if (!flush)
	Mapping::List::remove(m);
    }

  if (flush)
    {
      for (Mapping::List::Iterator i = f.frame->_root.begin();
           i != Mapping::List::end(); ++i)
	{
	  Obj::Entry *e = static_cast<Obj::Entry*>(*i);
	  if (e->ref_cnt()) // counted
	    --f.frame->_cnt;
	  e->invalidate();
	}
      f.frame->_root.clear();
    }

} // flush()


