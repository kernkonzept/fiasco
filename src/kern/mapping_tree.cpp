INTERFACE:

#include "config.h"
#include "l4_types.h"
#include "lock.h"
#include "mapping.h"
#include "types.h"

#include <unique_ptr.h>

class Ram_quota;

class Simple_tree_submap_ops
{
};

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


INTERFACE [!big_endian]:
//
// Mapping_tree
// FIXME: do we need this depending on endianess ?
struct Mapping_tree
{
  typedef Mapping::Page Page;
  typedef Mapping::Pfn Pfn;
  typedef Mapping::Pcnt Pcnt;

  enum Size_id
  {
    Size_id_min = 0,
    Size_id_max = 9		// can be up to 15 (4 bits)
  };
  // DATA
  unsigned _count: 16;		///< Number of live entries in this tree.
  unsigned _size_id: 4;		///< Tree size -- see number_of_entries().
  unsigned _empty_count: 11;	///< Number of dead entries in this tree.
                                //   XXX currently never read, except in
                                //   sanity checks

  unsigned _unused: 1;		// (make this 32 bits to avoid a compiler bug)

  Mapping _mappings[0];

};


INTERFACE [big_endian]:
//
// Mapping_tree
// FIXME: do we need this depending on endianess ?
struct Mapping_tree
{
  typedef Mapping::Page Page;
  typedef Mapping::Pfn Pfn;
  typedef Mapping::Pcnt Pcnt;

  enum Size_id
  {
    Size_id_min = 0,
    Size_id_max = 9		// can be up to 15 (4 bits)
  };
  // DATA
  unsigned _unused: 1;		// (make this 32 bits to avoid a compiler bug)
  unsigned _size_id: 4;		///< Tree size -- see number_of_entries().
  unsigned _empty_count: 11;	///< Number of dead entries in this tree.
                                //   XXX currently never read, except in
                                //   sanity checks

  unsigned _count: 16;		///< Number of live entries in this tree.
  Mapping _mappings[0];
};

INTERFACE:
//
// class Physframe
//

/** Array elements for holding frame-specific data. */
class Base_mappable
{
public:
  typedef Mapping_tree::Page Page;
  // DATA
  cxx::unique_ptr<Mapping_tree> tree;
  typedef ::Lock Lock;
  Lock lock;
}; // struct Physframe


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

PUBLIC inline
unsigned long
Simple_tree_submap_ops::mem_size(Treemap const *) const
{ return 0; }

PUBLIC inline
void
Simple_tree_submap_ops::grant(Treemap *, Space *, Page_number) const
{}

PUBLIC inline
Space *
Simple_tree_submap_ops::owner(Treemap const *) const
{ return 0; }

PUBLIC inline
bool
Simple_tree_submap_ops::is_partial(Treemap const *, Page_number, Page_number) const
{ return false; }

PUBLIC inline
void
Simple_tree_submap_ops::del(Treemap *) const
{}

PUBLIC inline
void
Simple_tree_submap_ops::flush(Treemap *, Page_number, Page_number) const
{}

//
// Mapping-tree allocators
//

enum Mapping_tree_size
{
  Size_factor = 4,
  Size_id_max = 9		// can be up to 15 (4 bits)
};

PUBLIC inline
Mapping_tree::Size_id
Mapping_tree::shrink()
{
  unsigned sid = _size_id - 1;
  while (sid > 0 && ((static_cast<unsigned>(_count) << 2) < ((unsigned)Size_factor << sid)))
    --sid;

  return Size_id(sid);
}

PUBLIC inline
Mapping_tree::Size_id
Mapping_tree::bigger()
{ return Mapping_tree::Size_id(_size_id + 1); }

template<int SIZE_ID>
struct Mapping_tree_allocator
{
  Kmem_slab a;
  enum
  {
    Elem_size = (Size_factor << SIZE_ID) * sizeof (Mapping)
                + ((sizeof(Mapping_tree) + Mapping::Alignment - 1)
                   & ~((unsigned long)Mapping::Alignment - 1))
  };

  Mapping_tree_allocator(Kmem_slab **array)
  : a(Elem_size, Mapping::Alignment, "Mapping_tree")
  { array[SIZE_ID] = &a; }
};

template<int SIZE_ID_MAX>
struct Mapping_tree_allocators;

template<>
struct Mapping_tree_allocators<0>
{
  Mapping_tree_allocator<0> a;
  Mapping_tree_allocators(Kmem_slab **array) : a(array) {}
};

template<int SIZE_ID_MAX>
struct Mapping_tree_allocators
{

  Mapping_tree_allocator<SIZE_ID_MAX> a;
  Mapping_tree_allocators<SIZE_ID_MAX - 1> n;

  Mapping_tree_allocators(Kmem_slab **array) : a(array), n(array) {}
};

static Kmem_slab *_allocators[Size_id_max + 1];
static Mapping_tree_allocators<Size_id_max> _mapping_tree_allocators(_allocators);

static
Kmem_slab *
allocator_for_treesize(int size)
{
  return _allocators[size];
}

//
// class Mapping_tree
//

PUBLIC static
inline NEEDS["space.h"]
Ram_quota *
Mapping_tree::quota(Space *space)
{
  return space->ram_quota();
}

PUBLIC
void*
Mapping_tree::operator new (size_t, Mapping_tree::Size_id size_id) throw()
{ return allocator_for_treesize(size_id)->alloc(); }

PUBLIC
void
Mapping_tree::operator delete (void* block)
{
  if (!block)
    return;

  // Try to guess right allocator object -- XXX unportable!
  Mapping_tree* t = static_cast<Mapping_tree*>(block);

  t->check_integrity();

  allocator_for_treesize(t->_size_id)->free(block);
}

PUBLIC //inline NEEDS[Mapping_depth, Mapping_tree::last]
Mapping_tree::Mapping_tree(Size_id size_id, Page page,
                           Space *owner)
{
  _count = 1;			// 1 valid mapping
  _size_id = size_id;   	// size is equal to Size_factor << 0
#ifndef NDEBUG
  _empty_count = 0;		// no gaps in tree representation
#endif

  _mappings[0].set_depth(Mapping::Depth_root);
  _mappings[0].set_page(page);
  _mappings[0].set_space(owner);

  _mappings[1].set_depth(Mapping::Depth_end);

  // We also always set the end tag on last entry so that we can
  // check whether it has been overwritten.
  last()->set_depth(Mapping::Depth_end);
}

PUBLIC
Mapping_tree::~Mapping_tree()
{
  // special case for copied mapping trees
  for (Mapping *m = _mappings; m < end() && !m->is_end_tag(); ++m)
    {
      if (!m->submap() && !m->unused())
        quota(m->space())->free(sizeof(Mapping));
    }
}

PUBLIC //inline NEEDS[Mapping_depth, Mapping_tree::last]
Mapping_tree::Mapping_tree(Size_id size_id, Mapping_tree* from_tree)
{
  _size_id = size_id;
  last()->set_depth(Mapping::Depth_end);

  copy_compact_tree(this, from_tree);
}

// public routines with inline implementations
PUBLIC inline NEEDS[Mapping_tree_size]
unsigned
Mapping_tree::number_of_entries() const
{
  return Size_factor << (unsigned long)_size_id;
}

PUBLIC inline
Mapping *
Mapping_tree::mappings()
{
  return & _mappings[0];
}

PUBLIC inline
bool
Mapping_tree::is_empty() const
{
  return _count == 0;
}

PUBLIC inline NEEDS[Mapping_tree::mappings, Mapping_tree::number_of_entries]
Mapping *
Mapping_tree::end()
{
  return mappings() + number_of_entries();
}

PUBLIC inline NEEDS[Mapping_tree::end]
Mapping *
Mapping_tree::last()
{
  return end() - 1;
}

// A utility function to find the tree header belonging to a mapping. 

/** Our Mapping_tree.
    @return the Mapping_tree we are in.
 */
PUBLIC static
Mapping_tree *
Mapping_tree::head_of(Mapping *m)
{
  while (m->depth() > Mapping::Depth_root)
    {
      // jump in bigger steps if this is not a free mapping
      if (m->depth() <= Mapping::Depth_max)
        {
          m -= m->depth();
          continue;
        }

      m--;
    }

  return reinterpret_cast<Mapping_tree *>
    (reinterpret_cast<char *>(m) - sizeof(Mapping_tree));
  // We'd rather like to use offsetof as follows, but it's unsupported
  // for non-POD classes.  So we instead rely on GCC's zero-length
  // array magic (sizeof(Mapping_tree) is the size of the header
  // without the array).
  // return reinterpret_cast<Mapping_tree *>
  //   (reinterpret_cast<char *>(m) - offsetof(Mapping_tree, _mappings));
}

/** Next mapping in the mapping tree.
    @param t head of mapping tree, if available
    @return the next mapping in the mapping tree.  If the mapping has
    children, it is the first child.  Otherwise, if the mapping has a
    sibling, it's the next sibling.  Otherwise, if the mapping is the
    last sibling or only child, it's the mapping's parent.
 */
PUBLIC inline
Mapping *
Mapping_tree::next(Mapping *m)
{
  for (m++; m < end() && ! m->is_end_tag(); m++)
    if (! m->unused())
      return m;

  return 0;
}

/** Next child mapping of a given parent mapping.  This
    function traverses the mapping tree like next(); however, it
    stops (and returns 0) if the next mapping is outside the subtree
    starting with parent.
    @param parent Parent mapping
    @return the next child mapping of a given parent mapping
 */
PUBLIC inline NEEDS[Mapping_tree::next]
Mapping *
Mapping_tree::next_child(Mapping *parent, Mapping *current_child)
{
  // Find the next valid entry in the tree structure.
  Mapping *m = next(current_child);

  // If we didn't find an entry, or if the entry cannot be a child of
  // "parent", return 0
  if (m == 0 || m->depth() <= parent->depth())
    return 0;

  return m;			// Found!
}

// This function copies the elements of mapping tree src to mapping
// tree dst, ignoring empty elements (that is, compressing the
// source tree.  In-place compression is supported.
PUBLIC static
void
Mapping_tree::copy_compact_tree(Mapping_tree *dst, Mapping_tree *src)
{
  unsigned src_count = src->_count; // Store in local variable before
                                    // it can get overwritten

  // Special case: cannot in-place compact a full tree
  if (src == dst && src->number_of_entries() == src_count)
    return;

  // Now we can assume the resulting tree will not be full.
  assert (src_count < dst->number_of_entries());

  dst->_count = 0;
#ifndef NDEBUG
  dst->_empty_count = 0;
#endif

  Mapping *d = dst->mappings();

  for (Mapping *s = src->mappings();
       s && !s->is_end_tag();
       s = src->next(s))
    {
      *d++ = *s;
      dst->_count += 1;
    }

  assert (dst->_count == src_count); // Same number of entries
  assert (d < dst->end());
  // Room for one more entry (the Mapping::Depth_end entry)

  d->set_depth(Mapping::Depth_end);
  dst->last()->set_depth(Mapping::Depth_end);
} // copy_compact_tree()

// Don't inline this function -- it eats a lot of stack space!
PUBLIC // inline NEEDS[Mapping::data, Mapping::unused, Mapping::is_end_tag,
       //              Mapping_tree::end, Mapping_tree::number_of_entries]
void
Mapping_tree::check_integrity(Space *owner = (Space*)-1)
{
  (void)owner;
#ifndef NDEBUG
  bool enter_ke = false;
  // Sanity checking
  if (// Either each entry is used
      !(number_of_entries() == static_cast<unsigned>(_count) + _empty_count
      // Or the last used entry is end tag
        || mappings()[_count + _empty_count].is_end_tag()))
    {
      printf("mapdb consistency error: "
             "%u == %u + %u || mappings()[%u + %u].is_end_tag()=%d\n",
             number_of_entries(), static_cast<unsigned>(_count), _empty_count,
             _count, _empty_count, mappings()[_count + _empty_count].is_end_tag());
      enter_ke = true;
    }

  Mapping *m = mappings();

  if (!(m->is_end_tag()   // When the tree was copied to a new one
        || (!m->unused()  // The first entry is never unused.
            && m->depth() == 0
            && (owner == (Space *)-1 || m->space() == owner))))
    {
      printf("mapdb corrupted: owner=%p\n"
             "  m=%p (end: %s depth: %u space: %p page: %lx)\n",
             owner, m, m->is_end_tag() ? "yes" : "no", m->depth(), m->space(),
             cxx::int_value<Page>(m->page()));
      enter_ke = true;
    }

  unsigned used = 0, dead = 0;

  while (m < end() && !m->is_end_tag())
    {
      if (m->unused())
        dead++;
      else
        used++;

      m++;
    }

  if ((enter_ke |= _count != used))
    printf("mapdb: _count=%u != used=%u\n", _count, used);
  if ((enter_ke |= _empty_count != dead))
    printf("mapdb: _empty_count=%u != dead=%u\n", _empty_count, dead);

  if (enter_ke)
    {
      printf("mapdb:    from %p on CPU%u\n",
             __builtin_return_address(0),
             cxx::int_value<Cpu_number>(current_cpu()));
      kdb_ke("mapdb");
    }

#endif // ! NDEBUG
}


/**
 * Use this function to reset a the tree to empty.
 *
 * In the case where a tree was copied to a new one you have to use 
 * this function to prevent the node iteration in the destructor.
 */
PUBLIC inline
void
Mapping_tree::reset()
{
  _count = 0;
  _empty_count = 0;
  _mappings[0].set_depth(Mapping::Depth_end);
}

PUBLIC inline NEEDS[Mapping_tree::next, <cassert>]
Treemap *
Mapping_tree::find_submap(Mapping *parent)
{
  assert (! parent->submap());

  // We need just one step to find a possible submap, because they are
  // always a parent's first child.
  Mapping* m = next(parent);

  if (m && m->submap())
    return m->submap();

  return 0;
}

PUBLIC inline NEEDS["ram_quota.h"]
Mapping *
Mapping_tree::allocate(Ram_quota *payer, Mapping *parent,
                       bool insert_submap = false)
{
  Auto_quota<Ram_quota> q(payer, sizeof(Mapping));
  if (EXPECT_FALSE(!q))
    return 0;

  // After locating the right place for the new entry, it will be
  // stored there (if this place is empty) or the following entries
  // moved by one entry.

  // We cannot continue if the last array entry is not free.  This
  // only happens if an earlier call to free() with this mapping tree
  // couldn't allocate a bigger array.  In this case, signal an
  // out-of-memory condition.
  if (EXPECT_FALSE (! last()->unused()))
    return 0;

  // If the parent mapping already has the maximum depth, we cannot
  // insert a child.
  if (EXPECT_FALSE (parent->depth() == Mapping::Depth_max))
    return 0;

  //allocation is done, so...
  q.release();

  Mapping *insert = parent + 1, *free = 0;
  // - Find an insertion point for the new entry. Acceptable insertion
  //   points are either before a sibling (same depth) or at the end
  //   of the subtree; for submap insertions, it's always before
  //   the first sibling.  "insert" keeps track of the last
  //   acceptable insertion point.
  // - Find a free entry in the array encoding the subtree ("free").
  //   There might be none; in this case, we stop at the end of the
  //   subtree.

  if (!insert_submap)
    for (; insert < end(); ++insert)
      {
        // End of subtree?  If we reach this point, this is our insert spot.
        if (insert->is_end_tag() || insert->depth() <= parent->depth())
          break;

        if (insert->unused())
          free = insert;
        else if (free		// Only look for insert spots after free
                 && insert->depth() <= parent->depth() + 1)
          break;
      }

  assert (insert);
  assert (free == 0 || (free->unused() && free < insert));

  // We now update "free" to point to a free spot that is acceptable
  // as well.

  if (free)
    {
      // "free" will be the latest free spot before the "insert" spot.
      // If there is anything between "free" and "insert", move it
      // upward to make space just before insert.
      while (free + 1 != insert)
        {
          *free = *(free + 1);
          free++;
        }

#ifndef NDEBUG
      // Tree-header maintenance
      _empty_count -= 1;	// Allocated dead entry
#endif
    }
  else				// free == 0
    {
      // There was no free spot in the subtree.  Move everything
      // downward until we have free space.  This is guaranteed to
      // succeed, because we ensured earlier that the last entry of
      // the array is free.

      free = insert;		// This will be the free spot

      // Find empty spot
      while (! insert->unused())
        insert++;

      // Tree maintenance: handle end tag, empty count
      if (insert->is_end_tag())
        {
          // Need to move end tag.
          if (insert + 1 < end())
            insert++;           // Arrange for copying end tag as well
        }
#ifndef NDEBUG
      else
        _empty_count -= 1;      // Allocated dead entry
#endif

      // Move mappings
      while (insert > free)
        {
          *insert = *(insert - 1);
          --insert;
        }
    }

  _count += 1;		// Adding an alive entry

  // found a place to insert new child (free).
  free->set_depth(insert_submap ? (unsigned)Mapping::Depth_submap
                                : parent->depth() + 1);

  return free;
}

PUBLIC inline NEEDS["ram_quota.h"]
void
Mapping_tree::free_mapping(Ram_quota *q, Mapping *m)
{
  assert (!m->unused() && !m->is_end_tag());
  q->free(sizeof(Mapping));
  m->set_unused();
  --_count;
}

PUBLIC template< typename SUBMAP_OPS >
void
Mapping_tree::flush(Mapping *parent, bool me_too,
                    Pcnt offs_begin, Pcnt offs_end,
                    SUBMAP_OPS const &submap_ops = SUBMAP_OPS())
{
  assert (! parent->unused());

  // This is easy to do: We just have to iterate over the array
  // encoding the tree.
  Mapping *start_of_deletions = parent;
  unsigned p_depth = parent->depth();
  unsigned deleted = 0;
#ifndef NDEBUG
  unsigned empty_elems_passed = 0;
#endif

  if (me_too)
    {
      free_mapping(quota(parent->space()), parent);
      deleted++;
    }
  else
    start_of_deletions++;

  unsigned m_depth = p_depth;

  for (Mapping* m = parent + 1;
       m < end() && ! m->is_end_tag();
       m++)
    {
      if (unsigned (m->depth()) <= p_depth)
        {
          // Found another data element -- stop deleting.  Since we
          // created holes in the tree representation, account for it.
#ifndef NDEBUG
          _empty_count += deleted;
#endif
          return;
        }

      if (m->unused())
        {
#ifndef NDEBUG
          empty_elems_passed++;
#endif
          continue;
        }

      Space *space;
      if (Treemap* submap = m->submap())
        {
          space = submap_ops.owner(submap);
          if (! me_too
              // Check for immediate child.  Note: It's a bad idea to
              // call m->parent() because that would iterate backwards
              // over already-deleted entries.
              && m_depth == p_depth
              && submap_ops.is_partial(submap, offs_begin, offs_end))
            {
              submap_ops.flush(submap, offs_begin, offs_end);

              start_of_deletions++;
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
      free_mapping(quota(space), m);
      deleted++;
    }

  // We deleted stuff at the end of the array -- move end tag
  if (start_of_deletions < end())
    {
      start_of_deletions->set_depth(Mapping::Depth_end);

#ifndef NDEBUG
      // also, reduce number of free entries
      _empty_count -= empty_elems_passed;
#endif
    }
}

PUBLIC template< typename SUBMAP_OPS >
bool
Mapping_tree::grant(Mapping* m, Space *new_space, Page page,
                    SUBMAP_OPS const &submap_ops = SUBMAP_OPS())
{
  unsigned long _quota = sizeof(Mapping);
  Treemap* submap = find_submap(m);
  if (submap)
    _quota += submap_ops.mem_size(submap);

  if (!quota(new_space)->alloc(_quota))
    return false;

  Space *old_space = m->space();

  m->set_space(new_space);
  m->set_page(page);

  if (submap)
    submap_ops.grant(submap, new_space, page);

  quota(old_space)->free(_quota);
  return true;
}

PUBLIC inline
Mapping *
Mapping_tree::lookup(Space *space, Page page)
{

  Mapping *m;

  for (m = mappings(); m; m = next(m))
    {
      assert (!m->submap());
      if (m->space() == space && m->page() == page)
        return m;
    }

  return 0;
}


PUBLIC
Mapping *
Base_mappable::lookup(Space *space, Page page)
{
  // get and lock the tree.
  if (EXPECT_FALSE(lock.lock() == Lock::Invalid))
    return 0;
  Mapping_tree *t = tree.get();
  assert (t);
  if (Mapping *m = t->lookup(space, page))
    return m;

  lock.clear();
  return 0;
}

PUBLIC inline
Mapping *
Base_mappable::insert(Mapping* parent, Space *space, Page page)
{
  Mapping_tree* t = tree.get();
  if (!t)
    {
      assert (parent == 0);
      Auto_quota<Ram_quota> q(Mapping_tree::quota(space), sizeof(Mapping));
      if (EXPECT_FALSE(!q))
        return 0;

      Mapping_tree::Size_id min_size = Mapping_tree::Size_id_min;
      cxx::unique_ptr<Mapping_tree> new_tree(new (min_size) Mapping_tree (min_size, page, space));

      if (EXPECT_FALSE(!new_tree))
        return 0;

      tree = cxx::move(new_tree);
      q.release();
      return tree->mappings();
    }

  Mapping *free = t->allocate(Mapping_tree::quota(space), parent, false);

  if (EXPECT_FALSE(!free))
        return 0;

  free->set_space(space);
  free->set_page(page);

  t->check_integrity();
  return free;
}


PUBLIC
void 
Base_mappable::pack()
{
  // Before we unlock the tree, we need to make sure that there is
  // room for at least one new mapping.  In particular, this means
  // that the last entry of the array encoding the tree must be free.

  // (1) When we use up less than a quarter of all entries of the
  // array encoding the tree, copy to a smaller tree.  Otherwise, (2)
  // if the last entry is free, do nothing.  Otherwise, (3) if less
  // than 3/4 of the entries are used, compress the tree.  Otherwise,
  // (4) copy to a larger tree.

  Mapping_tree *t = tree.get();
  bool maybe_out_of_memory = false;

  do // (this is not actually a loop, just a block we can "break" out of)
    {
      // (1) Do we need to allocate a smaller tree?
      if (t->_size_id > Mapping_tree::Size_id_min // must not be smallest size
          && (static_cast<unsigned>(t->_count) << 2) < t->number_of_entries())
        {
          Mapping_tree::Size_id sid = t->Mapping_tree::shrink();
          cxx::unique_ptr<Mapping_tree> new_t(new (sid) Mapping_tree(sid, t));

          if (new_t)
            {
              // ivalidate node 0 because we must not free the quota for it
              t->reset();
              t = new_t.get();

              // Register new tree.
              tree = cxx::move(new_t);

              break;
            }
        }

      // (2) Is last entry is free?
      if (t->last()->unused())
        break;			// OK, last entry is free.

      // Last entry is not free -- either compress current array
      // (i.e., move free entries to end of array), or allocate bigger
      // array.

      // (3) Should we compress the tree?
      // We also try to compress if we cannot allocate a bigger
      // tree because there is no bigger tree size.
      if (t->_count < (t->number_of_entries() >> 2)
                      + (t->number_of_entries() >> 1)
          || t->_size_id == Size_id_max) // cannot enlarge?
        {
          if (t->_size_id == Size_id_max)
            maybe_out_of_memory = true;

          Mapping_tree::copy_compact_tree(t, t); // in-place compression

          break;
        }

      // (4) OK, allocate a bigger array.

      Mapping_tree::Size_id sid = t->bigger();
      cxx::unique_ptr<Mapping_tree> new_t(new (sid) Mapping_tree(sid, t));

      if (new_t)
        {
          // ivalidate node 0 because we must not free the quota for it
          t->reset();
          t = new_t.get();

          // Register new tree. 
          tree = cxx::move(new_t);
        }
      else
        {
          // out of memory -- just do tree compression and hope that helps.
          maybe_out_of_memory = true;

          Mapping_tree::copy_compact_tree(t, t); // in-place compression
        }
    }
  while (false);

  // The last entry of the tree should now be free -- exept if we're
  // out of memory.
  assert (t->last()->unused() || maybe_out_of_memory);
  (void) maybe_out_of_memory;
}



