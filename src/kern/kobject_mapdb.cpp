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

#include "assert_opt.h"
#include "config.h"
#include "globals.h"
#include "std_macros.h"

/**
 * Locks kernel object that was looked up via capability index.
 *
 * \param obj       The kernel object to lock.
 * \param cap_addr  The capability index via which `obj` was looked up.
 * \param other     When acquiring the locks for an object pair (two cap slots),
 *                  both might refer to the same object. This case needs special
 *                  handling, so when locking the second slot, the result of
 *                  locking the first slot must be provided.
 *
 * \note `cap_addr._c` must always point to an existing `Obj_space::Entry` (cap
 *       slot), i.e. it must be ensured that the cap slot itself is not deleted
 *       before the Obj_space that contains it is deleted.
 *
 * \return Locked object.
 * \retval nullptr  Locking the object failed.
 */
PRIVATE static inline NEEDS["assert_opt.h"]
Kobject_mappable *
Kobject_mapdb::lock_obj(Kobject_iface *obj, // Phys_addr
                        Obj::Cap_addr cap_addr, // Va_addr
                        Kobject_mappable *other = nullptr)
{
  assert_opt(obj != nullptr);

  Obj::Entry const *const cap_slot = cap_addr._c;

  // The idea for safely locking a kernel object is that we must recheck that
  // cap slot is still valid (`cap_slot->valid()`) and still points to the
  // looked up kernel object (`cap_slot->obj() == obj`). Otherwise we cannot be
  // sure that the kernel object `obj` is still alive. We need to disable
  // interrupts for that temporarily, otherwise we can be preempted at any
  // time, which would make the checks futile.
  auto guard = lock_guard(cpu_lock);

  // Used both for initial check and recheck after preemption point.
  auto holds_same_obj = [obj, cap_slot]()
    {
      // Entry got invalidated?
      if (EXPECT_FALSE(!cap_slot->valid()))
        return false;

      // Entry still points to the same object?
      // Note that actually the memory pointed to by `obj` might have been
      // reallocated to a different object. In other words, the object initially
      // looked up as `obj` was freed and now a different object is allocated in
      // the memory of `obj`. If in addition that new object was put in the same
      // cap slot, we cannot detect here that the object was replaced.
      // However, that is fine, because if the object can be replaced
      // concurrently, it is up to timing anyway whether the initial lookup is
      // done before or after the object is replaced.
      // The only thing that MUST NOT happen is that we work on an invalid cap
      // slot or on freed `obj` memory that has been reallocated for something
      // entirely else, which is prevented by this and the subsequent checks.
      if (EXPECT_FALSE(cap_slot->obj() != obj))
        return false;

      return true;
    };

  if (EXPECT_FALSE(!holds_same_obj()))
    return nullptr;

  // The cap_slot is still valid, because of RCU we know that obj is alive and
  // stays alive at least until the next preemption point.
  Kobject_mappable *obj_mappable = obj->map_root();

  if (other != nullptr && obj_mappable == other)
    return other;

  for (;;)
    {
      // Acquiring a helping lock is a preemptible operation, therefore we need
      // to recheck validity of the cap slot after each possible preemption
      // point, which is why we set `RETURN_AFTER_HELPING=true`.
      auto status = obj_mappable->_lock.lock</* RETURN_AFTER_HELPING = */ true>();

      if (EXPECT_FALSE(status == Switch_lock::Invalid))
        // Someone removed the object from the cap slot.
        return nullptr;

      if (EXPECT_TRUE(status != Switch_lock::Retry))
        break;

      // After a possible preemption:
      // Let's assume that the `obj` initially looked up was freed and now a
      // different kernel object is allocated in the memory of `obj`, which
      // in addition is also put in the same cap slot. This is unlikely but
      // not impossible. The layout of the reallocated object might be
      // incompatible, i.e. its `obj->map_root()` can differ from the
      // `obj_mappable` whose lock we try to acquire.
      if (!holds_same_obj() || obj->map_root() != obj_mappable)
        return nullptr;

      // Retry to acquire the lock...
    }

  // Successfully acquired lock, now need to recheck if cap slot still
  // points to object.
  if (EXPECT_TRUE(holds_same_obj()))
    return obj_mappable;

  // Otherwise, someone replaced the object in the cap slot, so unlock
  // again, and return failure.
  obj_mappable->_lock.clear();
  return nullptr;
}

PUBLIC static inline NEEDS[Kobject_mapdb::lock_obj]
bool
Kobject_mapdb::lookup(Space const *, Vaddr va, Phys_addr obj,
                      Frame *out)
{
  if (Kobject_mappable *rn = lock_obj(obj, va))
    {
      out->m = va._c;
      out->frame = rn;
      return true;
    }

  return false;
}

PRIVATE static inline NEEDS[Kobject_mapdb::lock_obj]
bool
Kobject_mapdb::lock_cap_pair(Phys_addr obj1, Vaddr va1, Frame *frame1,
                             Phys_addr obj2, Vaddr va2, Frame *frame2)
{
  Kobject_mappable *rn1 = lock_obj(obj1, va1);
  if (!rn1)
    return false;

  Kobject_mappable *rn2 = lock_obj(obj2, va2, rn1);
  if (!rn2)
    {
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
