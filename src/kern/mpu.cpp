INTERFACE [mpu]:

#include <cxx/dlist>

#include "bitmap.h"
#include "l4_fpage.h"
#include "l4_msg_item.h"
#include "mem_layout.h"
#include "warn.h"

class Mpu_regions;

/**
 * Generic, implementation agnostic MPU region attributes.
 */
class Mpu_region_attr
{
  L4_fpage::Rights _rights;
  L4_msg_item::Memory_type _type;
  bool _enabled;

  constexpr Mpu_region_attr(L4_fpage::Rights rights,
                            L4_msg_item::Memory_type type,
                            bool enabled)
  : _rights(rights), _type(type), _enabled(enabled)
  {}

public:
  Mpu_region_attr() = default;

  constexpr bool operator == (Mpu_region_attr const &other)
  {
    return    _rights  == other._rights
           && _type    == other._type
           && _enabled == other._enabled;
  }

  constexpr bool operator != (Mpu_region_attr const &other)
  {
    return !operator==(other);
  }

  static constexpr Mpu_region_attr
  make_attr(L4_fpage::Rights rights,
            L4_msg_item::Memory_type type = L4_msg_item::Memory_type::Normal(),
            bool enabled = true)
  {
    return Mpu_region_attr(rights, type, enabled);
  }

  constexpr L4_fpage::Rights rights() const { return _rights; }
  constexpr L4_msg_item::Memory_type type() const { return _type; }
  constexpr bool enabled() const { return _enabled; }

  inline void add_rights(L4_fpage::Rights rights)
  {
    _rights |= rights;
  }

  inline void del_rights(L4_fpage::Rights rights)
  {
    _rights &= ~rights;
  }
};

/**
 * Bit mask of MPU regions.
 */
class Mpu_regions_mask : public Bitmap<Mem_layout::Mpu_regions>
{
public:
  Mpu_regions_mask()
  { clear_all(); }

  // Required to make it compatible with Mpu_regions_update::Updates
  using Bitmap<Mem_layout::Mpu_regions>::operator=;
};

/**
 * MPU region update result.
 *
 * Updating MPU regions requires that the changed regions are written back to
 * hardware. Because a single update can affect multiple regions, this class
 * tracks the affected ones.
 *
 * Additionally, an "error" state is possible if an update failed.
 */
class Mpu_regions_update
{
  // Use an additional error bit.
  typedef Bitmap<Mem_layout::Mpu_regions + 1> Updates;
  enum { Error_bit = Mem_layout::Mpu_regions };

  Updates _updates;

public:
  enum Error {
    Error_no_mem,     ///< Out of regions.
    Error_collision,  ///< New region collides with existing, incompatible one.
    Error_max
  };
  static_assert(Error_max <= 2, "Can store only two errors");

  Mpu_regions_update()
  { _updates.clear_all(); }

  /**
   * Construct an error "update".
   */
  explicit Mpu_regions_update(Error error)
  {
    _updates.clear_all();
    _updates.set_bit(Error_bit);
    _updates.bit(0, error == Error_collision);
  }

  Mpu_regions_update(Mpu_regions_update const &) = default;
  Mpu_regions_update &operator=(Mpu_regions_update const &) = default;

  /**
   * Add a region to the update set.
   */
  inline void set_updated(unsigned region)
  { _updates.set_bit(region); }

  /**
   * Combine updates.
   *
   * Errors are sticky, that is, if any of the operands is in the error state,
   * the result will also be in an error state. This is just a safety measure.
   * The caller should check each individual update operation for errors.
   */
  Mpu_regions_update &operator|=(Mpu_regions_update const &other)
  {
    _updates |= other._updates;
    return *this;
  }

  /**
   * Bool operator testing if update succeeded.
   */
  explicit operator bool() const
  { return !_updates[Error_bit]; }

  /**
   * Fetch update result.
   *
   * It is the responsibility of the caller to check for errors before.
   */
  Mpu_regions_mask value() const
  {
    Mpu_regions_mask ret;
    ret = _updates;
    return ret;
  }

  /**
   * Returns error code in case update failed.
   */
  Error error() const
  { return _updates[0] ? Error_collision : Error_no_mem; }
};

/**
 * Interface to the CPUs MPU.
 */
class Mpu
{
public:
  /**
   * Initialize MPU.
   *
   * Brings the MPU into a defined state. Called by platform code before any
   * regions are setup.
   */
  static void init();

  /**
   * Write back changes to hardware.
   *
   * \param regions  The Mpu_regions object that was updated.
   * \param touched  Impacted regions.
   * \param inplace  Update region directly instead of performing a safe
   *                 disable-update-enable sequence.
   */
  static void sync(Mpu_regions const &regions, Mpu_regions_mask const &touched,
                   bool inplace = false);

  /**
   * Update MPU with new regions list.
   *
   * Write back all `regions` into hardware.
   */
  static void update(Mpu_regions const &regions);

  /**
   * Get number of supported regions.
   */
  static unsigned regions();
};

/**
 * A single MPU region.
 *
 * Regions are organized as a sorted, double linked, cyclic list. The
 * architecture extends the struct with the actual data for the hardware. All
 * addresses are inclusive!
 */
struct Mpu_region : public cxx::D_list_item
{
  constexpr Mpu_region();
  Mpu_region(Mword start, Mword end, Mpu_region_attr attr);

  constexpr Mword start() const;
  constexpr Mword end() const;
  constexpr Mpu_region_attr attr() const;

  inline void start(Mword start);
  inline void end(Mword end);
  inline void attr(Mpu_region_attr attr);
  inline void disable();

  bool operator < (Mpu_region const &o) const
  { return end() < o.start(); }
};

/**
 * List of MPU regions.
 *
 * The size is determined by Mem_layout::Mpu_regions. Any updates of the list
 * need to be written back explicitly to hardware, either by Mpu::sync() or
 * Mpu::update().
 *
 * Usually, regions need to be aligned to the granularity of the MPU hardware.
 * This class does *not* perform checks to verify this invariant. This is the
 * responsibility of the caller.
 */
class Mpu_regions
{
  typedef cxx::D_list<Mpu_region> Region_list;

public:
  /**
   * Construct new MPU region list.
   *
   * \param reserved  Map of regions that are not allocatable.
   */
  explicit Mpu_regions(Mpu_regions_mask const &reserved)
  : _size(Mpu::regions()), _reserved(reserved)
  {
    if (_size > Mem_layout::Mpu_regions)
      _size = Mem_layout::Mpu_regions;
  }

  Mpu_region const &operator[](unsigned i) const
  { return _regions[i]; }

  Mpu_regions_mask used()     const { return _used_mask; }
  Mpu_regions_mask reserved() const { return _reserved; }
  unsigned         size()     const { return _size; }

private:
  unsigned index(Mpu_region const *r) const
  { return r - _regions; }

  Mpu_region *deref_iter(Region_list::Iterator iter) const
  { return iter != _used_list.end() ? *iter : nullptr; }

  Mpu_region *front() const
  { return deref_iter(_used_list.begin()); }

  Mpu_region *next(Mpu_region *r) const
  {
    auto iter = _used_list.iter(r);
    return deref_iter(++iter);
  }

  Mpu_region *prev(Mpu_region *r) const
  {
    auto iter = _used_list.iter(r);
    return iter != _used_list.begin() ? *(--iter) : nullptr;
  }

  Mpu_region *erase(Mpu_region *r)
  {
    _used_mask.clear_bit(index(r));
    r->disable();
    return deref_iter(_used_list.erase(_used_list.iter(r)));
  }

  enum Insert { After, Before, Back };

  void insert(Mpu_region *r, Insert mode, Mpu_region *pos)
  {
    _used_mask.set_bit(index(r));
    if (mode == After)
      _used_list.insert_after(r, _used_list.iter(pos));
    else if (mode == Before)
      _used_list.insert_before(r, _used_list.iter(pos));
    else
      _used_list.push_back(r);
  }

  unsigned _size;
  Mpu_regions_mask _reserved;
  Mpu_regions_mask _used_mask;  ///< Bit mask of occupied regions
  Region_list _used_list;       ///< Sorted list (by address) of used regions
  Mpu_region _regions[Mem_layout::Mpu_regions];
};

//---------------------------------------------------------------------------
IMPLEMENTATION [mpu]:

/**
 * Try to add a new region.
 *
 * If the new region overlaps with one or more existing regions, the page
 * attributes are checked and the existing regions are either extended or the
 * call fails.
 *
 * \param start  Start address of region.
 * \param end    End address of region.
 * \param attr   Region memory attributes.
 * \param join   Allow coalescing of adjacent regions with same attributes.
 * \param slot   Region index that shall be allocated or -1 to search for
 *               free entry.
 *
 * \return The mask of region slots that have been updated and that
 *         need to be synced to HW, or an error.
 */
PUBLIC inline NEEDS[Mpu_regions::extend, Mpu_regions::find_free]
Mpu_regions_update
Mpu_regions::add(Mword start, Mword end, Mpu_region_attr attr, bool join = true,
                 int slot = -1)
{
  // Find existing regions left and right of the new region. In case of a
  // collision the existing regions need to be extended and optimized.
  Mpu_region *left = nullptr, *right = nullptr;
  for (Mpu_region *i : _used_list)
    {
      if (i->end() < start)
        left = i;
      else if (end < i->start())
        {
          right = i;
          break;
        }
      else if (EXPECT_TRUE(join))
        return extend(i, attr, start, end); // Slow path in case of collisions
      else
        return Mpu_regions_update(Mpu_regions_update::Error_collision);
    }

  Mpu_regions_update updates;
  Mpu_region *r = nullptr;

  /*
   * Possibly join with left region. Can be safely extended if attributes
   * match because possible collisions were tested already above.
   */
  if (left && (left->end() + 1U == start) && left->attr() == attr && join)
    {
      updates.set_updated(index(left));
      left->end(end);
      r = left;
    }

  /*
   * Possibly join with right region. The new region might have already been
   * joined with the left region. In this case the right region is discarded.
   * Otherwise the right region is extended to the left.
   */
  if (right && (right->start() == end + 1U) && right->attr() == attr && join)
    {
      updates.set_updated(index(right));

      if (r) // r == left
        {
          r->end(right->end());
          erase(right);
        }
      else
        {
          right->start(start);
          r = right;
        }
    }

  // done in case we joined one of the existing regions
  if (r)
    return updates;

  // Could not join an existing region. We need to allocate a new slot.
  r = find_free(slot);
  if (!r)
    return Mpu_regions_update(Mpu_regions_update::Error_no_mem);

  r->start(start);
  r->end(end);
  r->attr(attr);

  // insert into sorted list
  if (left)
    insert(r, After, left);
  else if (right)
    insert(r, Before, right);
  else
    insert(r, Back, nullptr);

  updates.set_updated(index(r));
  return updates;
}

/**
 * Delete region.
 *
 * Deleting a part of a region will result in occupying another slot. If
 * this fails, the whole mapping will be removed.
 *
 * \param start      Start of deleted region
 * \param end        End of deleted regions
 * \param attr[out]  Optional output parameter that receives the attributes of
 *                   the deleted region.
 *
 * \return The mask of region slots that have been updated and that
 *         need to be synced to HW.
 */
PUBLIC inline NEEDS[Mpu_regions::find_free]
Mpu_regions_update
Mpu_regions::del(Mword start, Mword end, Mpu_region_attr *attr = nullptr)
{
  Mpu_regions_update updates;

  Mpu_region *i = front();
  while (i)
    {
      if (i->end() < start)
        {
          i = next(i);
          continue;
        }

      if (i->start() > end)
        break;

      updates.set_updated(index(i));
      if (attr)
          *attr = i->attr();

      if (i->start() >= start && i->end() <= end)
        {
          // region is fully covered -> delete
          i = erase(i);
        }
      else if (i->start() < start && i->end() > end)
        {
          // unmap range falls inside of region -> try to split
          Mpu_region *r = find_free();
          if (r)
            {
              updates.set_updated(index(r));

              r->attr(i->attr());
              r->start(end + 1U);
              r->end(i->end());

              i->end(start - 1U);

              insert(r, After, i);
            }
          else
            {
              // no further regions available -> delete whole region
              WARN("Dropped whole region [" L4_MWORD_FMT ":" L4_MWORD_FMT
                   "] while deleting  [" L4_MWORD_FMT ":" L4_MWORD_FMT "]\n",
                   i->start(), i->end(), start, end);
              i = erase(i);
            }
        }
      else if (i->start() < start)
        {
          if (i->end() >= start)
            i->end(start - 1U);
        }
      else
        {
          if (i->start() <= end)
            i->start(end + 1U);
        }
    }

  return updates;
}

/**
 * Find region covering the given address.
 *
 * \param addr  The search address.
 *
 * \return nullptr if no region was found, otherwise the pointer to the region.
 */
PUBLIC inline
Mpu_region const *
Mpu_regions::find(Mword addr) const
{
  for (auto const &i : _used_list)
    {
      if (addr >= i->start() && addr <= i->end())
        return i;
    }

  return nullptr;
}

/**
 * Dump regions list.
 */
PUBLIC
void
Mpu_regions::dump() const
{
  unsigned i = 0;
  while (i < _used_mask.size() && (i = _used_mask.ffs(i)))
    {
      auto attr = _regions[i - 1].attr();
      printf("[" L4_MWORD_FMT ".." L4_MWORD_FMT ", %c%c, %cR%c%c]@%u ",
             _regions[i - 1].start(),
             _regions[i - 1].end(),
             attr.enabled() ? '+' : '-',
             (attr.type() == L4_msg_item::Memory_type::Normal())
                ? 'N'
                : ((attr.type() == L4_msg_item::Memory_type::Uncached())
                    ? 'U' : 'B'),
             (attr.rights() & L4_fpage::Rights::U()) ? 'U' : '-',
             (attr.rights() & L4_fpage::Rights::W()) ? 'W' : '-',
             (attr.rights() & L4_fpage::Rights::X()) ? 'X' : '-',
             i);
    }
  printf("\n");
}

/**
 * Allocate a free region.
 *
 * If a specific slot is requrested it might be a reserved one.
 *
 * \param slot  Region number that shall be allocated. If negative, an empty
 *              region is searched.
 * \return Pointer to free region, nullptr otherwise.
 */
PRIVATE inline
Mpu_region*
Mpu_regions::find_free(int slot = -1)
{
  if (slot < 0)
    {
      Mpu_regions_mask avail = _reserved;
      avail |= _used_mask;
      avail.invert();
      unsigned i = avail.ffs(0);
      if (i == 0 || i > _size)
        return nullptr;

      return &_regions[i - 1];
    }
  else
    {
      if (_used_mask[slot])
        return nullptr;
      return &_regions[slot];
    }
}

/**
 * Slow path to extend an existing region(s).
 *
 * \param first   The leftmost region that overlaps (partially) with the new
 *                region.
 * \param attr    The attributes of the new region.
 * \param start   Start address (inclusive) of the new region
 * \param end     End address (inclusive) of the new region
 */
PRIVATE inline
Mpu_regions_update
Mpu_regions::extend(Mpu_region *first, Mpu_region_attr attr, Mword start,
                    Mword end)
{
  // Make sure all regions that overlap have compatible attributes. Only then
  // we are allowed to coalesce them.
  for (auto i = first; i != nullptr; i = next(i))
    {
      if (end < i->start())
        break;
      else if (i->attr() != attr)
        return Mpu_regions_update(Mpu_regions_update::Error_collision);
    }

  // The remainder of the function always works on the first overlapping
  // region. We know that all overlapping regions can be coalesced. So once the
  // current region covers [start,end] we're done.
  Mpu_regions_update updates;
  updates.set_updated(index(first));

  // Extend to the left? Will possibly merge with compatible adjacent region.
  // Note that we have to check the attributes again because the check at the
  // entry of the function only verifies overlapping regions, not the adjacent
  // ones.
  //
  // This needs to be done only once because we already start at the first
  // overlapping region.
  if (first->start() > start)
    {
      Mpu_region *left = prev(first);
      if (left && left->end() + 1U >= start && left->attr() == attr)
        {
          first->start(left->start());
          updates.set_updated(index(left));
          erase(left);
        }
      else
        first->start(start);
    }

  // Extend to the right? Possibly merge with adjacent region. Again, check the
  // attributes before merging because the rightmost, adjacent region might not
  // be compatible. Do it in a loop because multiple regions to the right might
  // be covered.
  while (first->end() < end)
    {
      Mpu_region *right = next(first);
      if (right && right->start() <= end + 1U && right->attr() == attr)
        {
          first->end(right->end());
          updates.set_updated(index(right));
          erase(right);
        }
      else
        first->end(end);
    }

  return updates;
}
