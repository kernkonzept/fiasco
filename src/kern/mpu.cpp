INTERFACE:

#include <assert.h>
#include "l4_types.h"
#include "warn.h"
#include "l4_fpage.h"
#include "l4_msg_item.h"
#include "mem_layout.h"

class Mpu_region;
class Mpu_regions;

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
    return    _rights  != other._rights
           || _type    != other._type
           || _enabled != other._enabled;
  }

  static constexpr Mpu_region_attr make_attr(L4_fpage::Rights rights,
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

class Mpu
{
public:

  static void init();
  static void sync(Mpu_regions *regions, Unsigned32 touched,
                   bool inplace = false);
  static void update(Mpu_regions *regions);
  static unsigned regions();
};

struct Mpu_region
{
  Mpu_region *prev, *next;

  bool operator < (Mpu_region const &o) const
  { return end() < o.start(); }

  void unlink()
  {
    prev->next = next;
    next->prev = prev;
  }
};

class Mpu_regions
{
public:
  enum {
    Max_size = Mem_layout::Mpu_regions,

    Error           = (1UL << 31),
    Error_no_mem    = 0xffffffffU,
    Error_collision = (0xffffffffU - 1U),
  };
  static_assert(Max_size < 32, "Can only handle up to 31 regions");

  explicit Mpu_regions(Unsigned32 reserved)
  : _size(Mpu::regions()), _reserved(reserved), _used(0)
  {
    if (_size > Max_size)
      _size = Max_size;
  }

  /**
   * Try to add a new region.
   *
   * If the new region overlaps with one or more existing regions the page
   * attributes are checked and the existing regions are either extended or the
   * call fails.
   *
   * @return The mask of region slots that have been updated and that
   *         need to be synced to HW. ~0U in case of error.
   */
  Unsigned32 add(Mword start, Mword end, Mpu_region_attr attr, bool join = true,
                 int slot = -1)
  {
    //assert(start < end);

    // Find existing regions left and right of the new region. In case of a
    // collision the existing regions need to be extended and optimized.
    Mpu_region *i, *left = nullptr, *right = nullptr;
    for (i = _head.next; i != &_head; i = i->next)
      {
        if (i->end() < start)
          left = i;
        else if (end < i->start())
          {
            right = i;
            break;
          }
        else
          {
            // Slow path in case of collisions
            if (i->attr() == attr && join)
              return extend(start, end);
            else
              return Error_collision;
          }
      }

    Mword touched = 0;
    Mpu_region *r = nullptr;

    /*
     * Possibly join with left region. Can be safely extended if attributes
     * match because possible collisions were tested already above.
     */
    if (left && (left->end() + 1U == start) && left->attr() == attr && join)
      {
        touched |= 1UL << (left - _regions);
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
        Unsigned32 mask = 1UL << (right - _regions);
        touched |= mask;

        if (r && r == left)
          {
            _used &= ~mask;
            right->disable();
            r->end(right->end());
            r->next = right->next;
            r->next->prev = r;
          }
        else
          {
            right->start(start);
            r = right;
          }
      }

    // done in case we joined one of the existing regions
    if (r)
      return touched;

    // Could not join an existing region. We need to allocate a new slot.
    r = find_free(slot);
    if (!r)
      return Error_no_mem;

    r->start(start);
    r->end(end);
    r->attr(attr);

    // insert into list
    r->prev = left ? left : &_head;
    r->prev->next = r;
    r->next = right ? right : &_head;
    r->next->prev = r;

    touched |= 1UL << (r - _regions);
    _used |= 1UL << (r - _regions);
    return touched;
  }

  /**
   * Delete region.
   *
   * Deleting a part of a region will result in occupying another slot. If
   * this fails the whole mapping will be removed. TODO: can we do better?
   *
   * @return The mask of region slots that have been updated and that
   *         need to be synced to HW. Zero in case of nothing matched.
   */
  Unsigned32 del(Mword start, Mword end, Mpu_region_attr *attr)
  {
    Unsigned32 touched = 0;

    for (auto i = _head.next; i != &_head; i = i->next)
      {
        if (i->end() < start)
          continue;
        if (i->start() > end)
          break;

        touched |= 1UL << (i - _regions);
        if (attr)
            *attr = i->attr();

        if (i->start() >= start && i->end() <= end)
          {
            // region is fully covered -> delete
            _used &= ~(1UL << (i - _regions));
            i->disable();
            i->prev->next = i->next;
            i->next->prev = i->prev;
          }
        else if (i->start() < start && i->end() > end)
          {
            // unmap range falls inside of region -> try to split
            Mpu_region *r = find_free();
            if (r)
              {
                touched |= 1UL << (r - _regions);
                _used |= 1UL << (r - _regions);

                r->attr(i->attr());
                r->start(end + 1U);
                r->end(i->end());
                r->next = i->next;
                r->next->prev = r;
                r->prev = i;

                i->end(start - 1U);
                i->next = r;
              }
            else
              {
                // no further regions available -> delete whole region
                WARN("Dropped whole region [" L4_MWORD_FMT ":" L4_MWORD_FMT "] while deleting  [" L4_MWORD_FMT ":" L4_MWORD_FMT "]\n",
                  i->start(), i->end(), start, end);
                _used &= ~(1UL << (i - _regions));
                i->disable();
                i->prev->next = i->next;
                i->next->prev = i->prev;
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

    return touched;
  }

  Mpu_region const *find(Mword addr) const
  {
    for (auto i = _head.next; i != &_head; i = i->next)
      {
        if (addr >= i->start() && addr <= i->end())
          return i;
      }

    return nullptr;
  }

  Mpu_region const * operator[](unsigned i) const
  {
    return &_regions[i];
  }

  Unsigned32 used()     const { return _used; }
  Unsigned32 reserved() const { return _reserved; }
  unsigned   size()     const { return _size; }

  void dump() const
  {
    for (Unsigned32 m = _used, i=0; m; m >>= 1, i++)
      {
        if (!(m & 1U))
          continue;
        auto attr = _regions[i].attr();
        printf("[" L4_MWORD_FMT ".." L4_MWORD_FMT ", %c%c, %cR%c%c]@%u ",
               _regions[i].start(),
               _regions[i].end(),
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

private:
  /**
   * Allocate a free region.
   *
   * If a specific slot is requrested it might be a reserved one.
   */
  Mpu_region* find_free(int slot = -1)
  {
    if (slot < 0)
      {
        Unsigned32 avail = ~(_reserved | _used);
        unsigned i = __builtin_ffsl(avail);
        if (i == 0 || i > _size)
          return NULL;

        return &_regions[i-1];
      }
    else
      {
        if (_used & (1UL << slot))
          return NULL;
        return &_regions[slot];
      }
  }

  Unsigned32 extend(Mword start, Mword end)
  {
    Unsigned32 touched = 0;

    Mpu_region r{start, end, Mpu_region_attr()};

    Mpu_region *i = _head.next;
    while (i != &_head)
      {
        if (*i < r)
          {
            i = i->next;
            continue;
          }
        else if (r < *i)
          {
            break;
          }

        // Extend to the left? Will possibly merge with adjacent region.
        if (i->start() > start)
          {
            Unsigned32 mask = 1UL << (i - _regions);
            touched |= mask;

            auto left = i->prev;
            if (left->end() + 1U >= start)
              {
                left->end(i->end());
                left->next = i->next;
                left->next->prev = left;
                _used &= ~mask;
                touched |= 1UL << (left - _regions);
                i->disable();
                i = left;
              }
            else
              i->start(start);
          }

        // Extend to the right? Possibly merge with adjacent region.
        if (i->end() < end)
          {
            touched |= 1UL << (i - _regions);

            auto right = i->next;
            if (right->start() <= end + 1U)
              {
                i->end(right->end());
                i->next = right->next;
                i->next->prev = i;
                Unsigned32 mask = 1UL << (right - _regions);
                _used &= ~mask;
                touched |= mask;
                right->disable();
              }
            else
              i->end(end);
          }

        i = i->next;
      }

    return touched;
  }

  unsigned _size;
  Mword _reserved;  // Needs to be a machine size word to prevent the compiler
  Mword _used;      // from generating unaligned accesses.
  Mpu_region _head;
  Mpu_region _regions[Max_size];
};

//------------------------------------------------------------------

INTERFACE [!mpu]:

#include "l4_fpage.h"
#include "l4_msg_item.h"

EXTENSION struct Mpu_region
{
private:
  Mword _start, _end;
  Mpu_region_attr _attr;

public:
  constexpr Mword start() { return _start; }
  constexpr Mword end()   { return _end; }
  constexpr Mpu_region_attr attr() { return _attr; }

  inline void start(Mword start) { _start = start; }
  inline void end(Mword end) { _end = end; }
  inline void attr(Mpu_region_attr attr) { _attr = attr; }

  constexpr Mpu_region()
  : prev(this), next(this), _start(~0UL), _end(0), _attr()
  {}

  Mpu_region(Mword start, Mword end, Mpu_region_attr attr)
  : prev(this), next(this), _start(start), _end(end), _attr(attr)
  {}
};


IMPLEMENT static inline
void Mpu::init()
{}

IMPLEMENT static inline
void Mpu::sync(Mpu_regions *, Unsigned32)
{}

IMPLEMENT static inline
void Mpu::update(Mpu_regions *)
{}

IMPLEMENT static inline
unsigned Mpu::regions()
{
  return 31;
}
