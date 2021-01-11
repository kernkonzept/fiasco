INTERFACE:

#include "config.h"
#include "l4_types.h"
#include "lock.h"
#include "mapping.h"
#include "space.h"
#include "types.h"
#include <cxx/slist>

class Ram_quota;

/* The mapping database.

 * This implementation encodes mapping trees in very compact arrays of
 * fixed sizes, prefixed by a tree header (Mapping_tree).  Array
 * sizes can vary from 4 mappings to 4<<15 mappings.  For each size,
 * we set up a slab allocator.  To grow or shrink the size of an
 * array, we have to allocate a larger or smaller tree from the
 * corresponding allocator and then copy the array elements.
 * 
 * The array elements (Mapping) contain a tree depth element.  This
 * depth and the relative position in the array is all information we
 * need to derive tree structure information.  Here is an example:
 * 
 * array
 * element   depth
 * number    value    comment
 * --------------------------
 * 0         0        Sigma0 mapping
 * 1         1        child of element #0 with depth 0
 * 2         2        child of element #1 with depth 1
 * 3         2        child of element #1 with depth 1
 * 4         3        child of element #3 with depth 2
 * 5         2        child of element #1 with depth 1
 * 6         3        child of element #5 with depth 2
 * 7         1        child of element #0 with depth 0
 * 
 * This array is a pre-order encoding of the following tree:
 *
 *                   0
 *                /     \
 *               1       7
 *            /  |  \
 *           2   3   5
 *               |   |
 *               4   6

 * The mapping database (Mapdb) is organized in a hierarchy of
 * frame-number-keyed maps of Mapping_trees (Treemap).  The top-level
 * Treemap contains mapping trees for superpages.  These mapping trees
 * may contain references to Treemaps for subpages.  (Original credits
 * for this idea: Christan Szmajda.)
 *
 *        Treemap
 *        -------------------------------
 *        | | | | | | | | | | | | | | | | array of ptr to 4M Mapping_tree's
 *        ---|---------------------------
 *           |
 *           v a Mapping_tree
 *           ---------------
 *           |             | tree header
 *           |-------------|
 *           |             | 4M Mapping *or* ptr to nested Treemap
 *           |             | e.g.
 *           |      ----------------| Treemap
 *           |             |        v array of ptr to 4K Mapping_tree's
 *           ---------------        -------------------------------
 *                                  | | | | | | | | | | | | | | | |
 *                                  ---|---------------------------
 *                                     |
 *                                     v a Mapping_tree
 *                                     ---------------
 *                                     |             | tree header
 *                                     |-------------|
 *                                     |             | 4K Mapping
 *                                     |             |
 *                                     |             |
 *                                     |             |
 *                                     ---------------

 * IDEAS for enhancing this implementation: 

 * We often have to find a tree header corresponding to a mapping.
 * Currently, we do this by iterating backwards over the array
 * containing the mappings until we find the Sigma0 mapping, from
 * whose address we can compute the address of the tree header.  If
 * this becomes a problem, we could add one more byte to the mappings
 * with a hint (negative array offset) where to find the sigma0
 * mapping.  (If the hint value overflows, just iterate using the hint
 * value of the mapping we find with the first hint value.)  Another
 * idea (from Adam) would be to just look up the tree header by using
 * the physical address from the page-table lookup, but we would need
 * to change the interface of the mapping database for that (pass in
 * the physical address at all times), or we would have to include the
 * physical address (or just the address of the tree header) in the
 * Mapdb-user-visible Mapping (which could be different from the
 * internal tree representation).  (XXX: Implementing one of these
 * ideas is probably worthwile doing!)

 * Instead of copying whole trees around when they grow or shrink a
 * lot, or copying parts of trees when inserting an element, we could
 * give up the array representation and add a "next" pointer to the
 * elements -- that is, keep the tree of mappings in a
 * pre-order-encoded singly-linked list (credits to: Christan Szmajda
 * and Adam Wiggins).  24 bits would probably be enough to encode that
 * pointer.  Disadvantages: Mapping entries would be larger, and the
 * cache-friendly space-locality of tree entries would be lost.
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
  ~Mapping_tree() { erase(nullptr); }

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
  using Lock = ::Lock;
  using Iterator = Mapping_tree::Iterator;

  Lock lock;

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


// Helpers

static Kmem_slab_t<Mapping> _mapping_allocator("Mapping");

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
      _mapping_allocator.q_del(q, m);
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
  Mapping *m = _mapping_allocator.q_new(payer);
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
  Mapping *m = _mapping_allocator.q_new(payer);
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
  _mapping_allocator.q_del(q, d);
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
      if (!m->submap() && ((int)m->depth() <= p_depth))
        return;

      Space *space;
      if (Treemap* submap = m->submap())
        {
          space = submap_ops.owner(submap);
          if (! me_too
              && m_depth == p_depth
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

PUBLIC inline
Mapping_tree::Iterator
Base_mappable::alloc_mapping(Ram_quota *q,
                             Mapping_tree::Iterator parent,
                             bool submap)
{
  if (submap)
    return _tree.allocate_submap(q, parent);
  else
    return _tree.allocate(q, parent);
}

