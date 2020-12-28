INTERFACE:

#include "slab_cache.h"
#include "l4_types.h"
#include "types.h"
#include "mapping.h"
#include "mapping_tree.h"
#include "auto_quota.h"

struct Mapping_tree;		// forward decls
class Physframe;
class Treemap;
class Space;

/** A mapping database.
 */
class Mapdb
{
  friend class Jdb_mapdb;

public:
  typedef ::Mapping Mapping;
  typedef Mapping::Pfn Pfn;
  typedef Mapping::Pcnt Pcnt;
  typedef Mdb_types::Order Order;

  // TYPES
  class Frame
  {
    friend class Mapdb;
    Treemap* treemap;
    Physframe* frame;

  public:
    inline Pfn vaddr(Mapping* m) const;
    inline Order page_shift() const;
  };

  // for mapdb_t
  Treemap *dbg_treemap() const
  { return _treemap; }

private:
  // DATA
  Treemap* const _treemap;
};

// 
// class Physframe
// 

/** Array elements for holding frame-specific data. */
class Physframe : public Base_mappable
{
  friend class Mapdb;
  friend class Treemap;
  friend class Jdb_mapdb;
}; // struct Physframe


// 
// class Treemap
// 

class Treemap
{
public:
  typedef Mapping_tree::Page Page;
  typedef Mapdb::Pfn Pfn;
  typedef Mapdb::Pcnt Pcnt;
  typedef Mapdb::Order Order;

private:
  friend class Jdb_mapdb;
  friend class Treemap_ops;

  // DATA
  Page _key_end;		///< Number of Physframe entries
  Space *_owner_id;	///< ID of owner of mapping trees 
  Pfn _page_offset;	///< Virt. page address in owner's addr space
  Order _page_shift;		///< Page size of mapping trees
  Physframe* _physframe;	///< Pointer to Physframe array
  const size_t* const _sub_shifts; ///< Pointer to array of sub-page sizes
  const unsigned _sub_shifts_max;  ///< Number of valid _page_sizes entries

public:
  Page end() const { return _key_end; }
  Pfn page_offset() const { return _page_offset; }
};


IMPLEMENTATION:

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
 * 	          /     \ 
 *               1       7
 *            /  |  \                   
 *           2   3   5
 *               |   |
 *        	 4   6
       	       	   
 * The mapping database (Mapdb) is organized in a hierarchy of
 * frame-number-keyed maps of Mapping_trees (Treemap).  The top-level
 * Treemap contains mapping trees for superpages.  These mapping trees
 * may contain references to Treemaps for subpages.  (Original credits
 * for this idea: Christan Szmajda.)
 *
 *        Treemap
 *        -------------------------------
 *     	  | | | | | | | | | | | | | | | | array of ptr to 4M Mapping_tree's
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
 *                             	       ---------------
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

#include <cassert>
#include <cstring>

#include "config.h"
#include "globals.h"
#include "helping_lock.h"
#include "kmem_alloc.h"
#include "kmem_slab.h"
#include "ram_quota.h"
#include "std_macros.h"
#include <new>

#if 0 // Optimization: do this using memset in Physframe::alloc()
inline
Physframe::Physframe ()
{}

inline
void *
Physframe::operator new (size_t, void *p) 
{ return p; }
#endif

PUBLIC static inline
unsigned long
Physframe::mem_size(size_t size)
{
  return (size*sizeof(Physframe) + 1023) & ~1023;
}

static inline
Physframe *
Physframe::alloc(size_t size)
{
#if 1				// Optimization: See constructor
  void *mem = Kmem_alloc::allocator()->unaligned_alloc(mem_size(size));
  if (mem)
    memset(mem, 0, size * sizeof(Physframe));
  return (Physframe *)mem;
#else
  Physframe* block
    = (Physframe *)Kmem_alloc::allocator()->unaligned_alloc(mem_size(size));
  assert (block);
  for (unsigned i = 0; i < size; ++i)
    new (block + i) Physframe();

  return block;
#endif
}

inline NOEXPORT
       NEEDS["mapping_tree.h", Treemap::~Treemap, Treemap::operator delete]
Physframe::~Physframe()
{
  if (has_mappings())
    {
      auto guard = lock_guard(lock);

      // Find next-level trees.
      for (Mapping* m = tree()->mappings();
	   m && !m->is_end_tag();
	   m = tree()->next(m))
	{
	  if (m->submap())
	    delete m->submap();
	}

      erase_tree();
    }
}

static // inline NEEDS[Physframe::~Physframe]
void 
Physframe::free(Physframe *block, size_t size)
{
  for (unsigned i = 0; i < size; ++i)
    block[i].~Physframe();

  Kmem_alloc::allocator()->unaligned_free(Physframe::mem_size(size), block);
}

//
// class Treemap_ops
//
// Helper operations for Treemaps (used in Mapping_tree::flush)

class Treemap_ops
{
  typedef Treemap::Order Order;
  typedef Treemap::Page Page;
  typedef Treemap::Pfn Pfn;
  typedef Treemap::Pcnt Pcnt;
  Order _page_shift;
};

PUBLIC inline
Treemap_ops::Treemap_ops(Order page_shift)
: _page_shift(page_shift)
{}

PUBLIC inline
Treemap::Pfn
Treemap_ops::to_vaddr(Page v) const
{ return Pfn(cxx::int_value<Page>(v << _page_shift)); }

PUBLIC inline
Treemap::Pcnt
Treemap_ops::page_size() const
{ return Pcnt(1) << _page_shift; }

PUBLIC
unsigned long
Treemap_ops::mem_size(Treemap const *submap) const
{
  unsigned long quota
    = cxx::int_value<Page>(submap->_key_end) * sizeof(Physframe) + sizeof(Treemap);

  for (Page key = Page(0);
      key < submap->_key_end;
      ++key)
    {
      Physframe *f = submap->frame(key);
      quota += f->base_quota_size();
      if (!f->has_mappings())
        continue;

      // recurse dow through all page sizes
      if (Treemap *s = f->find_submap(f->insertion_head()))
        quota += mem_size(s);
    }

  return quota;
}

PUBLIC
void
Treemap_ops::grant(Treemap *submap, Space *old_space,
                   Space *new_space, Page virt_page) const
{
  submap->_owner_id = new_space;
  submap->_page_offset = to_vaddr(virt_page);
  auto const end = submap->_key_end;
  auto const subva = virt_page << (_page_shift - submap->_page_shift);

  for (Page key = Page(0); key < end; ++key)
    {
      auto f = submap->frame(key);
      f->grant_tree(new_space, subva + key);
      if (!f->has_mappings())
        continue;

      // recurse dow through all page sizes
      if (Treemap *s = f->find_submap(f->insertion_head()))
        Treemap_ops(submap->_page_shift).grant(s, old_space, new_space,
                    subva + key);
    }
}

PUBLIC inline
Space *
Treemap_ops::owner(Treemap const *submap) const
{ return submap->owner(); }

PUBLIC inline
bool
Treemap_ops::is_partial(Treemap const *submap, Treemap::Pcnt offs_begin,
                        Treemap::Pcnt offs_end) const
{
  return offs_begin > Treemap::Pcnt(0)
         || offs_end < submap->size();
}

PUBLIC inline
void 
Treemap_ops::del(Treemap *submap) const
{ delete submap; }

PUBLIC inline NEEDS[Treemap::round_to_page, Treemap::trunc_to_page, "mapping_tree.h"]
void
Treemap_ops::flush(Treemap *submap,
                   Treemap::Pcnt offs_begin, Treemap::Pcnt offs_end) const
{
  for (Treemap::Page i = submap->trunc_to_page(offs_begin);
      i < submap->round_to_page(offs_end);
      ++i)
    {
      Pcnt page_offs_begin = submap->to_vaddr(i) - Pfn(0);
      Pcnt page_offs_end = page_offs_begin + submap->page_size();

      Physframe* subframe = submap->frame(i);

      if (!subframe->has_mappings())
        continue;

      auto guard = lock_guard(subframe->lock);

      if (offs_begin <= page_offs_begin && offs_end >= page_offs_end)
        subframe->erase_tree();
      else
        {
          // FIXME: do we have to check for subframe->tree != 0 here ?
          submap->flush(subframe,
                        page_offs_begin > offs_begin
                          ? Pcnt(0)
                          : cxx::get_lsb(offs_begin - page_offs_begin, _page_shift),
                        page_offs_end < offs_end
                          ? page_size()
                          : cxx::get_lsb(offs_end - page_offs_begin - Pcnt(1), _page_shift) + Pcnt(1));
        }
    }
}


//
// Treemap members
//

PRIVATE inline
Treemap::Treemap(Page key_end, Space *owner_id,
                 Pfn page_offset, Order page_shift,
                 const size_t* sub_shifts, unsigned sub_shifts_max,
                 Physframe *physframe)
  : _key_end(key_end),
    _owner_id(owner_id),
    _page_offset(page_offset),
    _page_shift(page_shift),
    _physframe(physframe),
    _sub_shifts(sub_shifts),
    _sub_shifts_max(sub_shifts_max)
{
  assert (_physframe);
}

PUBLIC inline
Treemap::Page
Treemap::round_to_page(Pcnt v) const
{
  return Page(cxx::int_value<Pcnt>((v + ((Pcnt(1) << _page_shift) - Pcnt(1)))
                                   >> _page_shift));
}

PUBLIC inline
Treemap::Page
Treemap::trunc_to_page(Pcnt v) const
{ return Page(cxx::int_value<Pcnt>(v >> _page_shift)); }

PUBLIC inline
Treemap::Page
Treemap::trunc_to_page(Pfn v) const
{ return Page(cxx::int_value<Pfn>(v >> _page_shift)); }

PUBLIC inline
Treemap::Pfn
Treemap::to_vaddr(Page v) const
{ return Pfn(cxx::int_value<Page>(v << _page_shift)); }

PUBLIC inline
Treemap::Pcnt
Treemap::page_size() const
{ return Pcnt(1) << _page_shift; }

PRIVATE static inline
unsigned long
Treemap::quota_size(Page key_end)
{ return Physframe::mem_size(cxx::int_value<Page>(key_end)) + sizeof(Treemap); }

PUBLIC static
Treemap *
Treemap::create(Order parent_page_shift, Space *owner_id,
                Pfn page_offset,
                const size_t* shifts, unsigned shifts_max)
{
  Order page_shift(shifts[0]);
  Page key_end = Page(1) << (parent_page_shift - page_shift);
  Auto_quota<Ram_quota> quota(Mapping_tree::quota(owner_id), quota_size(key_end));

  if (EXPECT_FALSE(!quota))
    return 0;

  Physframe *pf = Physframe::alloc(cxx::int_value<Page>(key_end));

  if (EXPECT_FALSE(!pf))
    return 0;

  void *m = allocator()->alloc();
  if (EXPECT_FALSE(!m))
    {
      Physframe::free(pf, cxx::int_value<Page>(key_end));
      return 0;
    }

  quota.release();
  return new (m) Treemap(key_end, owner_id, page_offset, page_shift, shifts + 1,
                         shifts_max - 1, pf);
}



PUBLIC inline NEEDS[Physframe::free, Mapdb]
Treemap::~Treemap()
{
  Physframe::free(_physframe, cxx::int_value<Page>(_key_end));
}

static Kmem_slab_t<Treemap> _treemap_allocator("Treemap");

static
Slab_cache *
Treemap::allocator()
{ return _treemap_allocator.slab(); }


PUBLIC inline NEEDS[Treemap::quota_size]
void
Treemap::operator delete (void *block)
{
  Treemap *t = reinterpret_cast<Treemap*>(block);
  Space *id = t->_owner_id;
  auto end = t->_key_end;
  asm ("" : "=m"(t->_owner_id), "=m"(t->_key_end));
  allocator()->free(block);
  Mapping_tree::quota(id)->free(Treemap::quota_size(end));
}

PUBLIC inline
Treemap::Order
Treemap::page_shift() const
{
  return _page_shift;
}

PUBLIC inline
Space *
Treemap::owner() const
{
  return _owner_id;
}


PUBLIC inline NEEDS[Treemap::to_vaddr, "mapping_tree.h"]
Treemap::Pcnt
Treemap::size() const
{
  return to_vaddr(_key_end) - Pfn(0);
}

PUBLIC inline NEEDS[Treemap::size, "mapping_tree.h"]
Treemap::Pfn
Treemap::end_addr() const
{
  return _page_offset + size();
}

PUBLIC inline NEEDS[Treemap::to_vaddr, "mapping_tree.h"]
Treemap::Pfn
Treemap::vaddr(Mapping* m) const
{
  return to_vaddr(m->page());
}

PUBLIC inline NEEDS[Treemap::trunc_to_page, "mapping_tree.h"]
void
Treemap::set_vaddr(Mapping* m, Pfn address) const
{
  m->set_page(trunc_to_page(address));
}

PUBLIC
Physframe*
Treemap::tree(Page key)
{
  assert (key < _key_end);

  Physframe *f = &_physframe[cxx::int_value<Page>(key)];

  f->lock.lock();
  if (!f->init_tree(key + trunc_to_page(_page_offset), _owner_id))
    {
      f->lock.clear();
      return 0;
    }

  return f;
}

PUBLIC
Physframe*
Treemap::frame(Page key) const
{
  assert (key < _key_end);

  return &_physframe[cxx::int_value<Page>(key)];
}

PUBLIC
bool
Treemap::lookup(Pcnt key, Space *search_space, Pfn search_va,
                Mapping** out_mapping, Treemap** out_treemap,
                Physframe** out_frame)
{
  // get and lock the tree.
  assert (trunc_to_page(key) < _key_end);
  Physframe *f = tree(trunc_to_page(key)); // returns locked frame

  // FIXME: should we return an OOM here ?
  if (!f)
    return false;

  Mapping_tree *t = f->tree();
  Order const psz = _page_shift;

  assert (t);
  assert (t->mappings()[0].space() == _owner_id);
  assert (vaddr(t->mappings()) == cxx::mask_lsb(key, psz) + _page_offset);

  Mapping *m;
  bool ret = false;

  for (m = t->mappings(); m; m = t->next (m))
    {
      if (Treemap *sub = m->submap())
	{
	  // XXX Recursion.  The max. recursion depth should better be
	  // limited!
	  if ((ret = sub->lookup(cxx::get_lsb(key, psz),
	                         search_space, search_va,
	                         out_mapping, out_treemap, out_frame)))
	    break;
	}
      else if (m->space() == search_space
	       && vaddr(m) == cxx::mask_lsb(search_va, psz))
	{
	  *out_mapping = m;
	  *out_treemap = this;
	  *out_frame = f;
	  return true;		// found! -- return locked
	}
    }

  // not found, or found in submap -- unlock tree
  f->lock.clear();

  return ret;
}

PUBLIC
Mapping *
Treemap::insert(Physframe* frame, Mapping* parent, Space *parent_space, Pfn parent_va,
                Space *space, Pfn va, Pcnt phys, Pcnt size)
{
  Treemap* submap = 0;
  Ram_quota *payer;
  Order const psz = _page_shift;
  bool insert_submap = page_size() != size;

  // Inserting subpage mapping?  See if we can find a submap.  If so,
  // we don't have to allocate a new Mapping entry.
  if (insert_submap)
    submap = frame->find_submap(parent);

  if (! submap) // Need allocation of new entry -- for submap or
		// normal mapping
    {
      // first check quota! In case of a new submap the parent pays for 
      // the node...
      payer = Mapping_tree::quota(insert_submap ? parent_space : space);

      Mapping *free = frame->alloc_mapping(payer, parent, insert_submap);
      if (EXPECT_FALSE(!free))
        return 0; 

      // Check for submap entry.
      if (! insert_submap)	// Not a submap entry
        {
          free->set_space(space);
          set_vaddr(free, va);

          frame->check_integrity(_owner_id);
          return free;
        }

      assert (_sub_shifts_max > 0);

      submap = Treemap::create(_page_shift, parent_space, parent_va,
                               _sub_shifts, _sub_shifts_max);
      if (! submap)
        {
          // free the mapping got with allocate
          frame->free_mapping(payer, free);
          return 0;
        }

      free->set_submap(submap);
    }

  Pcnt subframe_offset = cxx::mask_lsb(cxx::get_lsb(phys, psz), submap->page_shift());
  Physframe* subframe = submap->tree(submap->trunc_to_page(subframe_offset));
  if (! subframe)
    return 0;

  Mapping* ret = submap->insert(subframe, subframe->insertion_head(),
                                parent_space, parent_va + subframe_offset,
                                space, va,
                                cxx::get_lsb(phys, psz), size);

  subframe->release();

  return ret;
} // Treemap::insert()

PUBLIC
void
Treemap::flush(Physframe* f, Pcnt offs_begin, Pcnt offs_end)
{
  // This is easy to do: We just have to iterate over the array
  // encoding the tree.
  f->flush(offs_begin, offs_end, Treemap_ops(_page_shift));
  f->check_integrity(_owner_id);
  return;
} // Treemap::flush()

PUBLIC
void
Treemap::flush(Physframe* f, Mapping* parent, bool me_too,
               Pcnt offs_begin, Pcnt offs_end)
{
  assert (! parent->unused());

  // This is easy to do: We just have to iterate over the array
  // encoding the tree.
  f->flush(parent, me_too, offs_begin, offs_end, Treemap_ops(_page_shift));
  f->check_integrity(_owner_id);
  return;
} // Treemap::flush()

PUBLIC
bool
Treemap::grant(Physframe* f, Mapping* m, Space *new_space, Pfn va)
{
  return f->grant(m, new_space, trunc_to_page(va), Treemap_ops(_page_shift));
}



//
// class Mapdb_iterator
//

PUBLIC template<typename F> inline
void
Treemap::for_range(Page start, Page end, F &&func)
{
  Order ps = page_shift();

  for (Page sub_page = start; sub_page < end; ++sub_page)
    {
      Physframe *f = frame(sub_page);
      if (!f->has_mappings())
        continue;

      auto guard = lock_guard(f->lock);
      func(f, ps);
    }
}

PRIVATE template<typename F> static inline NEEDS[Physframe]
void
Mapdb::_for_full_subtree(Base_mappable *m, unsigned min_depth,
                         Mapping *first, Mapdb::Order size, F &&func)
{
  typedef Mapping::Page Page;
  auto *tree = m->tree();
  for (Mapping *cursor = first; cursor; cursor = tree->next(cursor))
    {
      if (Treemap *map = cursor->submap())
        map->for_range(Page(0), map->end(), [func](Physframe *f, Mapdb::Order size)
            {
              _for_full_subtree(f, f->min_depth(), f->first(), size,
                                func);

            });
      else if (cursor->depth() >= min_depth)
        func(cursor, size);
      else
        break;
    }
}

PRIVATE template<typename F> static inline NEEDS[Treemap::round_to_page, Physframe]
void
Mapdb::_foreach_mapping(Base_mappable *mappable, unsigned min_depth,
                        Mapping *cursor,
                        Mapdb::Order size,
                        Mapdb::Pfn va_begin, Mapdb::Pfn va_end, F &&func)
{
  typedef Mapping::Page Page;
  auto tree = mappable->tree();

  if (!cursor)
    return;

  if (Treemap *submap = cursor->submap())
    {
      Order ps = submap->page_shift();
      Pfn po = submap->page_offset();
      Page start(0);
      Page end(submap->end());

      if (va_begin > po)
        start = Page(cxx::int_value<Pcnt>((va_begin - po) >> ps));

      if (va_end < po + submap->size())
        end = submap->round_to_page(va_end - po);

      submap->for_range(start, end, [&func, va_begin, va_end](Physframe *f, Mapdb::Order size)
          {
            _foreach_mapping(f, f->min_depth(), f->first(),
                             size, va_begin, va_end, func);
          });

      cursor = tree->next(cursor);
    }

  _for_full_subtree(mappable, min_depth, cursor, size, cxx::forward<F>(func));
}

PUBLIC template<typename F> static inline NEEDS[Treemap::round_to_page, Physframe]
void
Mapdb::foreach_mapping(Mapdb::Frame const &f, Mapping* parent,
                       Mapdb::Pfn va_begin, Mapdb::Pfn va_end, F &&func)
{
  _foreach_mapping(f.frame, parent->depth() + 1, f.frame->tree()->next(parent),
                   f.treemap->page_shift(),
                   va_begin, va_end, cxx::forward<F>(func));
}


// 
// class Mapdb
// 

/** Constructor.
    @param End physical end address of RAM.  (Used only if 
           Config::Mapdb_ram_only is true.) 
 */
PUBLIC
Mapdb::Mapdb(Space *owner, Order parent_page_shift, size_t const *page_shifts,
             unsigned page_shifts_max)
: _treemap(Treemap::create(parent_page_shift, owner,
                           Pfn(0), page_shifts, page_shifts_max))
{
  // assert (boot_time);
  assert (_treemap);
} // Mapdb()

/** Destructor. */
PUBLIC
Mapdb::~Mapdb()
{
  delete _treemap;
}

/** Insert a new mapping entry with the given values as child of
    "parent".
    We assume that there is at least one free entry at the end of the
    array so that at least one insert() operation can succeed between a
    lookup()/free() pair of calls.  This is guaranteed by the free()
    operation which allocates a larger tree if the current one becomes
    to small.
    @param parent Parent mapping of the new mapping.
    @param space  Number of the address space into which the mapping is entered
    @param va     Virtual address of the mapped page.
    @param size   Size of the mapping.  For memory mappings, 4K or 4M.
    @return If successful, new mapping.  If out of memory or mapping 
           tree full, 0.
    @post  All Mapping* pointers pointing into this mapping tree,
           except "parent" and its parents, will be invalidated.
 */
PUBLIC
Mapping *
Mapdb::insert(const Mapdb::Frame& frame, Mapping* parent, Space *space,
              Pfn va, Pfn phys, Pcnt size)
{
  return frame.treemap->insert(frame.frame, parent, parent->space(),
                               frame.treemap->vaddr(parent),
                               space, va, phys - Pfn(0), size);
} // insert()


/** 
 * Lookup a mapping and lock the corresponding mapping tree.  The returned
 * mapping pointer, and all other mapping pointers derived from it, remain
 * valid until free() is called on one of them.  We guarantee that at most 
 * one insert() operation succeeds between one lookup()/free() pair of calls 
 * (it succeeds unless the mapping tree is fu68,ll).
 * @param space Number of virtual address space in which the mapping 
 *              was entered
 * @param va    Virtual address of the mapping
 * @param phys  Physical address of the mapped pag frame
 * @return mapping, if found; otherwise, 0
 */
PUBLIC
bool
Mapdb::lookup(Space *space, Pfn va, Pfn phys,
             Mapping** out_mapping, Mapdb::Frame* out_lock)
{
  return _treemap->lookup(phys - Pfn(0), space, va, out_mapping,
                          & out_lock->treemap, & out_lock->frame);
}

/** Unlock the mapping tree to which the mapping belongs.  Once a tree
    has been unlocked, all Mapping instances pointing into it become
    invalid.

    A mapping tree is locked during lookup().  When the tree is
    locked, the tree may be traversed (using member functions of
    Mapping, which serves as an iterator over the tree) or
    manipulated (using insert(), free(), flush(), grant()).  Note that
    only one insert() is allowed during each locking cycle.

    @param mapping_of_tree Any mapping belonging to a mapping tree.
 */
PUBLIC
static void
Mapdb::free(const Mapdb::Frame& f)
{
  f.frame->release();
} // free()

/** Delete mappings from a tree.  This function deletes mappings
    recusively.
    @param m Mapping that denotes the subtree that should be deleted.
    @param me_too If true, delete m as well; otherwise, delete only 
           submappings.
 */
PUBLIC static
void
Mapdb::flush(const Mapdb::Frame& f, Mapping *m, L4_map_mask mask,
             Pfn va_start, Pfn va_end)
{
  Pcnt size = f.treemap->page_size();
  Pcnt offs_begin = va_start > f.treemap->vaddr(m)
                  ? va_start - f.treemap->vaddr(m)
                  : Pcnt(0);
  Pcnt offs_end = va_end > f.treemap->vaddr(m) + size
                ? size
                : va_end - f.treemap->vaddr(m);

  f.treemap->flush(f.frame, m, mask.self_unmap(), offs_begin, offs_end);
} // flush()

/** Change ownership of a mapping.
    @param m Mapping to be modified.
    @param new_space Number of address space the mapping should be 
                     transferred to
    @param va Virtual address of the mapping in the new address space
 */
PUBLIC
bool
Mapdb::grant(const Mapdb::Frame& f, Mapping *m, Space *new_space,
             Pfn va)
{
  return f.treemap->grant(f.frame, m, new_space, va);
}

/** Return page size of given mapping and frame. */
PUBLIC static inline NEEDS[Treemap::page_shift, "mapping_tree.h"]
Mapdb::Order
Mapdb::shift(const Mapdb::Frame& f, Mapping * /*m*/)
{
  // XXX add special case for _mappings[0]: Return superpage size.
  return f.treemap->page_shift();
}

PUBLIC static inline NEEDS[Treemap::vaddr, "mapping_tree.h"]
Mapdb::Pfn
Mapdb::vaddr(const Mapdb::Frame& f, Mapping* m)
{
  return f.treemap->vaddr(m);
}

// 
// class Mapdb::Frame
// 

IMPLEMENT inline NEEDS[Treemap::vaddr, "mapping_tree.h"]
Mapdb::Pfn
Mapdb::Frame::vaddr(Mapping* m) const
{ return treemap->vaddr(m); }

IMPLEMENT inline NEEDS[Treemap::page_shift, "mapping_tree.h"]
Mapdb::Order
Mapdb::Frame::page_shift() const
{ return treemap->page_shift(); }

PUBLIC inline NEEDS [Treemap::end_addr, "mapping_tree.h"]
bool
Mapdb::valid_address(Pfn phys)
{
  // on the root level physical and virtual frame numbers
  // are the same
  return phys < _treemap->end_addr();
}


PUBLIC inline
Mapdb::Mapping *
Mapdb::check_for_upgrade(Pfn phys,
                         Space *from_id, Pfn snd_addr,
                         Space *to_id, Pfn rcv_addr,
                         Frame *mapdb_frame)
{
  // Check if we can upgrade mapping.  Otherwise, flush target
  // mapping.
  Mapping* receiver_mapping;
  if (valid_address(phys) // Can lookup in mapdb
      && lookup(to_id, rcv_addr, phys, &receiver_mapping, mapdb_frame))
    {
      Mapping* receiver_parent = receiver_mapping->parent();
      if (receiver_parent->space() == from_id
	  && vaddr(*mapdb_frame, receiver_parent) == snd_addr)
	return receiver_parent;
      else		// Not my child -- cannot upgrade
	free(*mapdb_frame);
    }
  return 0;
}


