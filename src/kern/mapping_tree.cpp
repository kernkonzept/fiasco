INTERFACE:

#include "config.h"
#include "l4_types.h"
#include "helping_lock.h"
#include "mapping.h"
#include "space.h"
#include "types.h"
#include <cxx/slist>

class Ram_quota;

/**
 * A mapping tree.
 *
 * This implementation encodes mapping trees as singly-linked lists (see
 * #Mapping_tree and #Mapping). The list elements (#Mapping) have an associated
 * tree depth. This depth and the relative position in the list is all
 * information we need to derive tree structure information. Here is an example:
 *
 * list index | depth | comment
 * :--------: | :---: | :-------------------------------
 *     0      |   0   | Sigma0 mapping
 *     1      |   1   | child of element #0 with depth 0
 *     2      |   2   | child of element #1 with depth 1
 *     3      |   2   | child of element #1 with depth 1
 *     4      |   3   | child of element #3 with depth 2
 *     5      |   2   | child of element #1 with depth 1
 *     6      |   3   | child of element #5 with depth 2
 *     7      |   1   | child of element #0 with depth 0
 *
 * This list is a pre-order encoding of the following tree:
 *
 *            0
 *          /   \
 *         1     7
 *      /  |  \
 *     2   3   5
 *         |   |
 *         4   6
 */
class Mapping_tree : public cxx::S_list<Mapping>
{
public:
  using Page = Mapping::Page;
  using Pfn  = Mapping::Pfn;
  using Pcnt = Mapping::Pcnt;

private:
  using Mappings = cxx::S_list<Mapping>;

public:
  ~Mapping_tree() { assert(!front()); }

  static Iterator insertion_head() { return Iterator(); }
  static Ram_quota *quota(Space *space) { return space->ram_quota(); }
};

//
// class Physframe
//

/** Array elements for holding frame-specific data. */
class Base_mappable
{
private:
  Mapping_tree _tree;

public:
  using Page = Mapping_tree::Page;
  using Pfn  = Mapping::Pfn;
  using Pcnt = Mapping::Pcnt;
  using Iterator = Mapping_tree::Iterator;

  Helping_lock lock;

  Mapping_tree *tree() { return &_tree; }
  Mapping_tree const *tree() const { return &_tree; }
  void erase_tree(Space *owner) { _tree.erase(owner); }
  bool has_mappings() const { return _tree.front(); }
  unsigned long base_quota_size() const { return 0; }
  Treemap *find_submap(Iterator parent) const
  { return _tree.find_submap(parent); }

  Iterator free_mapping(Ram_quota *q, Iterator m)
  { return _tree.free_mapping(q, m); }

  Iterator insertion_head() const
  { return _tree.insertion_head(); }

  void release() { lock.clear(); }
  unsigned min_depth() const { return 0; }
  Iterator first() const
  { return const_cast<Mapping_tree &>(_tree).begin(); }
};

//---------------------------------------------------------------------------
IMPLEMENTATION:

#include <cassert>
#include <cstring>

#include "config.h"
#include "globals.h"
#include "kdb_ke.h"
#include "kmem_slab.h"
#include "ram_quota.h"
#include "space.h"
#include "std_macros.h"
#include "global_data.h"


// Helpers

static DEFINE_GLOBAL
Global_data<Kmem_slab_t<Mapping>> _mapping_allocator("Mapping");

//
// class Mapping_tree
//

PUBLIC
void
Mapping_tree::erase(Space *owner)
{
  if (!front())
    return;

  Ram_quota *q = quota(owner);
  for (Iterator d = begin(); *d;)
    {
      // space is nullptr if the Mapping references a submap and
      // in this case trhe predecessor is the parent mapping that
      // contains the space pointer for this submap, so just use
      // the quota from the previous iteration.
      if (d->space())
        q = quota(d->space());

      assert(q);

      Mapping *m = *d;
      d = Mappings::erase(d);
      _mapping_allocator->q_del(q, m);
    }
}

PUBLIC inline NEEDS[<cassert>]
Treemap *
Mapping_tree::find_submap(Const_iterator parent) const
{
  // We need just one step to find a possible submap, because they are
  // always a parent's first child.

  if (!*parent) // == insertion_head())
    parent = begin();
  else
    ++parent;

  if (*parent)
    return parent->submap();

  return nullptr;
}

PUBLIC
Mapping_tree::Iterator
Mapping_tree::allocate_submap(Ram_quota *payer, Iterator parent)
{
  Mapping *m = _mapping_allocator->q_new(payer);
  if (!m)
    return end();

  if (*parent)
    {
      insert(m, parent);
      return ++parent;
    }
  else
    {
      push_front(m);
      return begin();
    }
}

PUBLIC
Mapping_tree::Iterator
Mapping_tree::allocate(Ram_quota *payer, Iterator parent)
{
  if (*parent && parent->has_max_depth())
    return end();

  Mapping *m = _mapping_allocator->q_new(payer);
  if (!m)
    return end();

  Iterator test = parent;
  if (*test)
    {
      m->set_depth(parent->depth() + 1);
      ++test;
    }
  else
    {
      m->set_depth(0);
      test = begin();
    }


  if (*test && test->submap())
    parent = test;

  if (*parent)
    {
      insert(m, parent);
      return ++parent;
    }
  else
    {
      push_front(m);
      return begin();
    }
}

PUBLIC
Mapping_tree::Iterator
Mapping_tree::free_mapping(Ram_quota *q, Iterator m)
{
  auto d = *m;
  m = Mappings::erase(m);
  _mapping_allocator->q_del(q, d);
  return m;
}

PUBLIC template< typename SUBMAP_OPS >
void
Mapping_tree::flush(Iterator m, int p_depth, bool me_too,
                    Pcnt offs_begin, Pcnt offs_end,
                    SUBMAP_OPS const &submap_ops = SUBMAP_OPS())
{
  int m_depth = p_depth;

  while (*m)
    {
      if (!m->submap() && static_cast<int>(m->depth()) <= p_depth)
        return;

      Space *space;
      if (Treemap* submap = m->submap())
        {
          space = submap_ops.owner(submap);
          // We never split mappings, therefore only a submap directly below the
          // root mapping of the flush, and submaps directly below that submap
          // (and so on recursively) can be flushed partially. All other
          // (overlapping) mappings are deleted entirely.
          // When the root mapping itself is deleted (me_too=true), partial
          // flushing does not make sense, and instead all descendant mappings
          // are deleted.
          if (! me_too
              && m_depth == p_depth // only for submap directly below the parent
                                    // (either root mapping of flush or Treemap root)
              && submap_ops.is_partial(submap, offs_begin, offs_end))
            {
              submap_ops.flush(submap, offs_begin, offs_end);
              ++m;
              continue;
            }
          else
            submap_ops.del(submap);
        }
      else    // Found a real mapping
        {
          space = m->space();
          m_depth = m->depth();
        }

      // Delete the element.
      m = free_mapping(quota(space), m);
    }
}

PUBLIC template< typename SUBMAP_OPS >
void
Mapping_tree::flush(Iterator parent, bool me_too,
                    Pcnt offs_begin, Pcnt offs_end,
                    SUBMAP_OPS &&submap_ops = SUBMAP_OPS())
{
  int p_depth;
  Iterator m = parent;
  if (*m)
    {
      p_depth = m->depth();
      if (me_too)
        m = free_mapping(quota(parent->space()), m);
      else
        ++m;
    }
  else
    {
      m = begin();
      p_depth = -1;
      me_too = false; // we do not support unmapping from sigma0, so downgrade...
    }

  flush(m, p_depth, me_too, offs_begin, offs_end,
        cxx::forward<SUBMAP_OPS>(submap_ops));
}

PUBLIC template< typename SUBMAP_OPS > inline
bool
Mapping_tree::grant(Iterator const &m, Space *new_space, Page page,
                    SUBMAP_OPS const &submap_ops = SUBMAP_OPS())
{
  unsigned long _quota = sizeof(Mapping);
  Treemap* submap = find_submap(m);

  Space *old_space = m->space();

  if (old_space != new_space)
    {
      if (submap)
        _quota += submap_ops.mem_size(submap);

      if (!quota(new_space)->alloc(_quota))
        return false;

      quota(old_space)->free(_quota);

      m->set_space(new_space);
    }

  m->set_page(page);

  if (submap)
    submap_ops.grant(submap, old_space, new_space, page);

  return true;
}

/**
 * Grant the mapping `m` of this mappable to a new destination.
 */
PUBLIC template< typename SUBMAP_OPS > inline
bool
Base_mappable::grant(Mapping_tree::Iterator m, Space *new_space, Page page,
                     SUBMAP_OPS &&submap_ops)
{
  return _tree.grant(m, new_space, page, cxx::forward<SUBMAP_OPS>(submap_ops));
}

PUBLIC template< typename SUBMAP_OPS > inline
void
Base_mappable::flush(Pcnt offs_begin, Pcnt offs_end,
                     SUBMAP_OPS &&submap_ops)
{
  // Pass `min_depth() - 1` as depth to ensure that the `m_depth == p_depth`
  // condition for partial submap flush in `Mapping_tree::flush()` is only
  // fulfilled for the top-level submap in this Treemap.
  _tree.flush(first(), min_depth() - 1, false, offs_begin, offs_end,
              cxx::forward<SUBMAP_OPS>(submap_ops));
}

PUBLIC template< typename SUBMAP_OPS > inline
void
Base_mappable::flush(Mapping_tree::Iterator parent, bool me_too,
                     Pcnt offs_begin, Pcnt offs_end,
                     SUBMAP_OPS &&submap_ops)
{
  _tree.flush(parent, me_too, offs_begin, offs_end,
              cxx::forward<SUBMAP_OPS>(submap_ops));
}
