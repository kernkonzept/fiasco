INTERFACE:

#include "slab_cache.h"
#include "l4_types.h"
#include "types.h"
#include "mapping.h"
#include "mapping_tree.h"
#include "auto_quota.h"

class Space;

//
// class Physframe
//

/** Array elements for holding frame-specific data. */
class Physframe : public Base_mappable
{
  friend class Mapdb;
  friend class Treemap;
  friend class Jdb_mapdb;

public:
  static constexpr unsigned long mem_size(size_t size)
  { return (size * sizeof(Physframe) + 1023) & ~1023; }

  static constexpr Bytes mem_bytes(size_t size)
  { return Bytes(mem_size(size)); }

}; // struct Physframe


//
// class Treemap
//

class Treemap
{
public:
  using Mapping = ::Mapping;
  using Page    = Mapping_tree::Page;
  using Pfn     = Mapping::Pfn;
  using Pcnt    = Mapping::Pcnt;
  using Order   = Mdb_types::Order;

private:
  bool match(Mapping const *m, Space *spc, Pfn va) const
  {
    return (m->space() == spc)
           && (vaddr(m) == cxx::mask_lsb(va, _page_shift));
  }

  static Bytes quota_size(Page key_end)
  {
    return Bytes(Physframe::mem_size(cxx::int_value<Page>(key_end))
                     + sizeof(Treemap));
  }

  // get the virtual address coresponding to the given physframe
  // inside this map
  Pfn frame_vaddr(Physframe const *f) const
  {
    return  _page_offset + (Pcnt(f - _physframe) << _page_shift);
  }

public:
  class Frame
  {
  public:
    using Iterator = Mapping_tree::Iterator;

    Iterator m;
    Treemap *treemap;
    Physframe *frame = nullptr;

    Pfn vaddr(Mapping_tree::Iterator m) const
    { return treemap->vaddr(*m); }

    Pfn vaddr() const
    { return treemap->vaddr(*m); }

    Order page_shift() const
    { return treemap->page_shift(); }

    bool same_lock(Frame const &o) const
    {
      return frame == o.frame;
    }

    void clear()
    {
      frame->lock.clear();
      frame = nullptr;
    }

    void clear_both(Physframe *f)
    {
      if (f != frame)
        f->lock.clear();

      clear();
    }

    void might_clear()
    {
      if (frame)
        clear();
    }

    void set(Mapping_tree::Iterator ma, Treemap *tm, Physframe *pf)
    {
      m = ma;
      treemap = tm;
      frame = pf;
    }

    Space *pspace() const
    { return *m ? m->space() : treemap->owner(); }

    Pfn pvaddr() const
    { return *m ? vaddr() : treemap->frame_vaddr(frame); }

    int next_depth() const
    { return *m ? m->depth() + 1 : 0; }

    Iterator next_mapping() const
    { return *m ? ++Iterator(m) : frame->first(); }
  };

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
  const unsigned _sub_shifts_num;  ///< Number of valid _sub_shifts entries

public:
  Treemap(Page key_end, Space *owner_id,
          Pfn page_offset, Order page_shift,
          const size_t* sub_shifts, unsigned sub_shifts_num,
          Physframe *physframe)
  : _key_end(key_end),
    _owner_id(owner_id),
    _page_offset(page_offset),
    _page_shift(page_shift),
    _physframe(physframe),
    _sub_shifts(sub_shifts),
    _sub_shifts_num(sub_shifts_num)
  {}

  ~Treemap()
  { Physframe::free(_physframe, cxx::int_value<Page>(_key_end), owner()); }

  Page  end() const         { return _key_end; }
  Pfn   page_offset() const { return _page_offset; }
  Pcnt  page_size() const   { return Pcnt(1) << _page_shift; }
  Order page_shift() const  { return _page_shift; }
  Space *owner() const      { return _owner_id; }

  Page round_to_page(Pcnt v) const
  {
    return Page(cxx::int_value<Pcnt>(
          (v + ((Pcnt(1) << _page_shift) - Pcnt(1)))
          >> _page_shift));
  }

  Page trunc_to_page(Pcnt v) const
  { return Page(cxx::int_value<Pcnt>(v >> _page_shift)); }

  Page trunc_to_page(Pfn v) const
  { return Page(cxx::int_value<Pfn>(v >> _page_shift)); }

  Pfn to_vaddr(Page v) const
  { return Pfn(cxx::int_value<Page>(v << _page_shift)); }

  Pcnt size() const
  { return to_vaddr(_key_end) - Pfn(0); }

  Pfn end_addr() const
  { return _page_offset + size(); }

  Pfn vaddr(Mapping const *m) const
  { return to_vaddr(m->page()); }

  void set_vaddr(Mapping* m, Pfn address) const
  { m->set_page(trunc_to_page(address)); }

  Physframe *frame(Page key) const
  { return &_physframe[cxx::int_value<Page>(key)]; }
};

/** A mapping database.
 */
class Mapdb
{
  friend class Jdb_mapdb;

public:
  using Mapping = ::Mapping;
  using Pfn =     Treemap::Pfn;
  using Pcnt =    Treemap::Pcnt;
  using Order =   Treemap::Order;
  using Frame =   Treemap::Frame;

  ~Mapdb()
  { delete _treemap; }

  Treemap *dbg_treemap() const
  { return _treemap; }

  bool valid_address(Pfn phys) const
  {
    // on the root level physical and virtual frame numbers
    // are the same
    return phys < _treemap->end_addr();
  }

private:
  // DATA
  Treemap *const _treemap;
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

#include "assert_opt.h"
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

static inline
Physframe *
Physframe::alloc(size_t size)
{
#if 1				// Optimization: See constructor
  void *mem = Kmem_alloc::allocator()->alloc(mem_bytes(size));
  if (mem)
    memset(mem, 0, size * sizeof(Physframe));
  return (Physframe *)mem;
#else
  Physframe* block
    = (Physframe *)Kmem_alloc::allocator()->alloc(mem_bytes(size));
  assert (block);
  for (unsigned i = 0; i < size; ++i)
    new (block + i) Physframe();

  return block;
#endif
}

inline NOEXPORT NEEDS["mapping_tree.h", Treemap::operator delete]
void
Physframe::del(Space *owner)
{
  if (has_mappings())
    {
      auto guard = lock_guard(lock);

      // Find next-level trees.
      for (auto m = tree()->begin(); *m; ++m)
        {
          if (m->submap())
            delete m->submap();
        }

      erase_tree(owner);
    }
}

inline NOEXPORT NEEDS["mapping_tree.h", <cassert>]
Physframe::~Physframe()
{
  assert (!has_mappings());
}

static // inline NEEDS[Physframe::~Physframe]
void 
Physframe::free(Physframe *block, size_t size, Space *owner)
{
  for (unsigned i = 0; i < size; ++i)
    {
      block[i].del(owner);
      block[i].~Physframe();
    }

  Kmem_alloc::allocator()->free(Physframe::mem_bytes(size), block);
}

//
// class Treemap_ops
//
// Helper operations for Treemaps (used in Mapping_tree::flush)

class Treemap_ops
{
private:
  using Order = Treemap::Order;
  using Page  = Treemap::Page;
  using Pfn   = Treemap::Pfn;
  using Pcnt  = Treemap::Pcnt;
  Order _page_shift;

public:
  explicit Treemap_ops(Order page_shift)
  : _page_shift(page_shift)
  {}

  Pfn to_vaddr(Page v) const
  { return Pfn(cxx::int_value<Page>(v << _page_shift)); }

  Pcnt page_size() const
  { return Pcnt(1) << _page_shift; }

  Space *owner(Treemap const *submap) const
  { return submap->owner(); }

  bool is_partial(Treemap const *submap, Pcnt offs_begin,
                  Pcnt offs_end) const
  {
    return offs_begin > Pcnt(0)
           || offs_end < submap->size();
  }

  void del(Treemap *submap) const
  { delete submap; }
};

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

      // recurse down through all page sizes
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
      if (!f->has_mappings())
        continue;

      // recurse down through all page sizes
      if (Treemap *s = f->find_submap(f->insertion_head()))
        Treemap_ops(submap->_page_shift).grant(s, old_space, new_space,
                    subva + key);
    }
}


PUBLIC inline NEEDS["mapping_tree.h"]
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
        subframe->erase_tree(submap->owner());
      else
        {
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

PUBLIC static
Treemap *
Treemap::create(Order parent_page_shift, Space *owner_id,
                Pfn page_offset,
                const size_t* shifts, unsigned shifts_num)
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
      Physframe::free(pf, cxx::int_value<Page>(key_end), owner_id);
      return 0;
    }

  quota.release();
  return new (m) Treemap(key_end, owner_id, page_offset, page_shift, shifts + 1,
                         shifts_num - 1, pf);
}

static Kmem_slab_t<Treemap> _treemap_allocator("Treemap");

static
Slab_cache *
Treemap::allocator()
{ return _treemap_allocator.slab(); }


PUBLIC inline
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

PUBLIC inline NEEDS[<cassert>]
Physframe*
Treemap::tree(Page key)
{
  assert (key < _key_end);

  Physframe *f = frame(key);
  f->lock.lock();
  return f;
}

PRIVATE inline
Mapping_tree::Iterator
Treemap::_lookup(Physframe *f, Mapping_tree::Iterator m, Space *spc, Pcnt key, Pfn va,
                 Frame *res, int *root_depth = 0, int *current_depth = 0)
{
  auto subkey = cxx::get_lsb(key, _page_shift);
  for (; *m; ++m)
    {
      if (Treemap *sub = m->submap())
        {
          // XXX Recursion.  The max. recursion depth should better be
          // limited!
          if (sub->lookup(subkey, spc, va, res))
            return m;

          continue;
        }

      if (current_depth)
        {
          *current_depth = m->depth();
          if (*root_depth >= *current_depth)
            *root_depth = -3;
        }

      if (match(*m, spc, va))
        {
          res->set(m, this, f);
          return m;
        }
    }
  return m;
}

PUBLIC
bool
Treemap::lookup(Pcnt key, Space *search_space, Pfn search_va,
                Frame *res)
{
  // get and lock the tree.
  assert (trunc_to_page(key) < _key_end);
  Physframe *f = tree(trunc_to_page(key)); // returns locked frame

  // FIXME: should we return an OOM here ?
  if (!f)
    return false;

  // special sigma0 case for synthetic 1. level mapping nodes
  if (search_space->is_sigma0())
    {
      assert (_owner_id == search_space);
      res->set(f->insertion_head(), this, f);
      return true;
    }

  auto m = _lookup(f, f->tree()->begin(), search_space, key, search_va, res);
  if (*m)
    {
      if (m->submap())
        f->lock.clear();

      return true;
    }

  // not found, or found in submap -- unlock tree
  f->lock.clear();

  return false;
}

PUBLIC inline NEEDS[Treemap::_lookup, "assert_opt.h"]
int
Treemap::lookup_src_dst(Space *src, Pcnt src_key, Pfn src_va, Pcnt src_size,
                        Space *dst, Pcnt dst_key, Pfn dst_va, Pcnt dst_size,
                        Frame *src_frame, Frame *dst_frame)
{
  assert_opt (!src_frame->frame);
  assert_opt (!dst_frame->frame);

  Order const ps = _page_shift;
  auto const src_k = trunc_to_page(src_key);
  auto const dst_k = trunc_to_page(dst_key);

  assert (src_k < _key_end);
  assert (dst_k < _key_end);

  if (src_k != dst_k)
    {
      // different phys frames, do separate lookups
      if (!lookup(dst_key, dst, dst_va, dst_frame))
        return -1;

      if (!lookup(src_key, src, src_va, src_frame))
        {
          // src mapping not found -> we cannot map
          // free the dst frame and abort
          dst_frame->clear();
          return -1;
        }

      // src and dst found but no upgrade possible
      return 1; // --> unmap dst then map
    }

  // we give prio to the dst mapping
  Physframe *f = tree(dst_k);
  auto t = f->tree();

  int c_depth = f->min_depth() - 1; // depth of current node
  Mapping_tree::Iterator m = t->begin();

  // special sigma0 case for synthetic 1. level mapping nodes
  if (*m && src->is_sigma0())
    {
      assert (src == _owner_id);
      src_frame->set(f->insertion_head(), this, f);
    }

  for (; *m && !src_frame->frame && !dst_frame->frame; ++m)
    {
      if (Treemap *sub = m->submap())
        {
          auto const dst_subkey = cxx::get_lsb(dst_key, ps);
          auto const src_subkey = cxx::get_lsb(src_key, ps);
          int r = sub->lookup_src_dst(src, src_subkey, src_va, src_size,
                                      dst, dst_subkey, dst_va, dst_size,
                                      src_frame, dst_frame);
          if (r >= 0)
            {
              f->lock.clear();
              return r;
            }

          continue;
        }

      c_depth = m->depth();

      if (match(*m, src, src_va))
        src_frame->set(m, this, f);

      if (match(*m, dst, dst_va))
        dst_frame->set(m, this, f);
    }

  if (!src_frame->frame && !dst_frame->frame)
    {
      f->lock.clear();
      return -1; // nothing found
    }
  else if (src_frame->frame && dst_frame->frame)
    // src == dst
    return 2; // unmap, but no map
  else if (src_frame->frame)
    {
      // look for dst only
      int r_depth = c_depth;
      m = _lookup(f, m, dst, dst_key, dst_va, dst_frame,
                  &r_depth, &c_depth);
      if (!*m)
        {
          src_frame->clear_both(f);
          return -1; // nothing found
        }

      if (!m->submap())
        {
          if (r_depth + 1 == c_depth)
            return 0; // upgrade

          return 1;
        }

        if (r_depth == c_depth
            && dst_frame->m->depth() == f->min_depth())
          return 0;

        // found dst and src, do not unlock because src_frame
        // needs to be kept locked
        return 1;
    }
  else
    {
      // src not yet found, look for src only
      int r_depth = c_depth;
      m = _lookup(f, m, src, src_key, src_va, src_frame,
                  &r_depth, &c_depth);

      if (!*m)
        {
          // nothing found
          dst_frame->clear_both(f);
          return -1;
        }

      if (!m->submap())
        {
          // same mapping size
          if (r_depth < (int)f->min_depth() - 1)
            return 1; // in another subtree -> unmap + map

          return 2; // in the same subtree -> unmap + no map
        }

      // smaller mapping in submap found...
      if (r_depth == c_depth)
        {
          // src is in subtree of dst -> unmap dst and noting to map then
          src_frame->clear();
          return 2; // unmap + no map
        }
      // src is in a sibling subtree -> unmap + map
      // needs to be kept locked
      return 1;
    }
}

PUBLIC
Mapping *
Treemap::insert(Physframe* frame, Mapping_tree::Iterator const &parent,
                Space *parent_space, Pfn parent_va,
                Space *space, Pfn va, Pcnt phys, Pcnt size)
{
  using Iterator = Mapping_tree::Iterator;
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

      Iterator free = frame->alloc_mapping(payer, parent, insert_submap);
      if (EXPECT_FALSE(!*free))
        return 0; 

      // Check for submap entry.
      if (! insert_submap)	// Not a submap entry
        {
          free->set_space(space);
          set_vaddr(*free, va);
          return *free;
        }

      assert (_sub_shifts_num > 0);

      submap = Treemap::create(_page_shift, parent_space, parent_va,
                               _sub_shifts, _sub_shifts_num);
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
  return;
} // Treemap::flush()

PUBLIC
void
Treemap::flush(Physframe* f, Mapping_tree::Iterator parent,
               bool me_too,
               Pcnt offs_begin, Pcnt offs_end)
{
  // This is easy to do: We just have to iterate over the array
  // encoding the tree.
  f->flush(parent, me_too, offs_begin, offs_end, Treemap_ops(_page_shift));
  return;
} // Treemap::flush()

PUBLIC
bool
Treemap::grant(Physframe* f, Mapping_tree::Iterator m, Space *new_space, Pfn va)
{
  return f->grant(m, new_space, trunc_to_page(va), Treemap_ops(_page_shift));
}

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

PRIVATE template<typename F> static inline
void
Mapdb::_for_full_subtree(unsigned min_depth,
                         Mapping_tree::Iterator first,
                         Mapdb::Order size,
                         F &&func)
{
  typedef Mapping::Page Page;
  for (auto cursor = first; *cursor; ++cursor)
    {
      if (Treemap *map = cursor->submap())
        map->for_range(Page(0), map->end(), [func](Physframe *f, Mapdb::Order size)
            {
              _for_full_subtree(f->min_depth(), f->first(), size,
                                func);

            });
      else if (cursor->depth() >= min_depth)
        func(*cursor, size);
      else
        break;
    }
}

PRIVATE template<typename F> static inline
void
Mapdb::_foreach_mapping(unsigned min_depth,
                        Mapping_tree::Iterator cursor,
                        Mapdb::Order size,
                        Mapdb::Pfn va_begin, Mapdb::Pfn va_end,
                        F &&func)
{
  typedef Mapping::Page Page;

  if (!*cursor)
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
            _foreach_mapping(f->min_depth(), f->first(),
                             size, va_begin, va_end, func);
          });

      ++cursor;
    }

  _for_full_subtree(min_depth, cursor, size, cxx::forward<F>(func));
}

PUBLIC template<typename F> static inline
void
Mapdb::foreach_mapping(Frame const &f,
                       Mapdb::Pfn va_begin, Mapdb::Pfn va_end,
                       F &&func)
{
  _foreach_mapping(f.next_depth(), f.next_mapping(),
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
PUBLIC inline NEEDS[<cassert>]
Mapdb::Mapdb(Space *owner, Order parent_page_shift, size_t const *page_shifts,
             unsigned page_shifts_num)
: _treemap(Treemap::create(parent_page_shift, owner,
                           Pfn(0), page_shifts, page_shifts_num))
{
  // assert (boot_time);
  assert (_treemap);
} // Mapdb()

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
PUBLIC inline
Mapping *
Mapdb::insert(Frame const &frame, Space *space,
              Pfn va, Pfn phys, Pcnt size)
{
  return frame.treemap->insert(frame.frame, frame.m,
                               frame.pspace(), frame.pvaddr(),
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
PUBLIC inline
bool
Mapdb::lookup(Space *space, Pfn va, Pfn phys,
              Frame *res)
{
  return _treemap->lookup(phys - Pfn(0), space, va, res);
}

/** Delete mappings from a tree.  This function deletes mappings
    recusively.
    @param m Mapping that denotes the subtree that should be deleted.
    @param me_too If true, delete m as well; otherwise, delete only 
           submappings.
 */
PUBLIC static inline
void
Mapdb::flush(Frame const &f, L4_map_mask mask,
             Pfn va_start, Pfn va_end)
{
  Pcnt size = f.treemap->page_size();
  Pcnt offs_begin = va_start > f.pvaddr()
                  ? va_start - f.pvaddr()
                  : Pcnt(0);
  Pcnt offs_end = va_end > f.pvaddr() + size
                ? size
                : va_end - f.pvaddr();

  f.treemap->flush(f.frame, f.m, mask.self_unmap(), offs_begin, offs_end);
} // flush()

/** Change ownership of a mapping.
    @param m Mapping to be modified.
    @param new_space Number of address space the mapping should be 
                     transferred to
    @param va Virtual address of the mapping in the new address space
 */
PUBLIC inline
bool
Mapdb::grant(Frame const &f, Space *new_space,
             Pfn va)
{
  return f.treemap->grant(f.frame, f.m, new_space, va);
}

PUBLIC inline
int
Mapdb::lookup_src_dst(Space *src, Pfn src_phys, Pfn src_va, Pcnt src_size,
                      Space *dst, Pfn dst_phys, Pfn dst_va, Pcnt dst_size,
                      Frame *src_frame, Frame *dst_frame)
{
  return _treemap->lookup_src_dst(src, src_phys - Pfn(0), src_va, src_size,
                                  dst, dst_phys - Pfn(0), dst_va, dst_size,
                                  src_frame, dst_frame);
}


