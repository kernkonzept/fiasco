INTERFACE[mapdb]:

#include "slab_cache.h"
#include "l4_types.h"
#include "types.h"
#include "mapping.h"
#include "mapping_tree.h"
#include "auto_quota.h"
#include "unique_ptr.h"

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
  bool match(Mapping const *m, Space const *spc, Pfn va) const
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
  friend class Mem_mapdb_test;

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

  Treemap *dbg_treemap() const
  { return _treemap.get(); }

  bool valid_address(Pfn phys) const
  {
    // on the root level physical and virtual frame numbers
    // are the same
    return phys < _treemap->end_addr();
  }

private:
  // DATA
  cxx::unique_ptr<Treemap> const _treemap;
};

//--------------------------------------------------------------------
INTERFACE[!mapdb]:

#include "l4_types.h"
#include "mapping.h"

class Mapdb
{
public:
  using Mapping = ::Mapping;
  using Pfn =     Mapping::Pfn;
  using Pcnt =    Mapping::Pcnt;
  using Order =   Mdb_types::Order;

  class Frame
  {
  public:
    static constexpr void *frame = nullptr;
    bool same_lock(Frame const &) const { return false; }
    void clear() {}
    void might_clear() {}
  };
};

//--------------------------------------------------------------------
IMPLEMENTATION[mapdb]:

/** \file
 * The mapping database.
 *
 * \internal
 * # Implementation details
 *
 * A mapping database (#Mapdb) is organized in a hierarchy of frame-number-keyed
 * maps (#Treemap) to mapping trees (#Mapping_tree, #Mapping). The
 * top-level #Treemap contains mapping trees for the largest page granularity.
 * These mapping trees may contain references to #Treemap instances for
 * subpages. (Original credits for this idea: Christan Szmajda.)
 *
 *     Treemap
 *     -------------------------------
 *     | | | | | | | | | | | | | | | | array of ptr to 4M Mapping_tree's
 *     ---|---------------------------
 *        |
 *        v a Mapping_tree
 *       ---    ---    ---    ---    ---    ---    ---
 *       | |--->| |--->| |--->| |--->| |--->| |--->| |
 *       ---    ---    -|-    ---    ---    ---    ---
 *                      | 4M Mapping *or* ptr to nested Treemap
 *                      | e.g.
 *                      ---------| Treemap
 *                               v array of ptr to 4K Mapping_tree's
 *                               -------------------------------
 *                               | | | | | | | | | | | | | | | |
 *                               ---|---------------------------
 *                                  |
 *                                  v a Mapping_tree
 *                                 ---    ---    ---    ---
 *                                 | |--->| |--->| |--->| |
 *                                 ---    ---    ---    ---

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
 * ideas is probably worthwhile doing!)
 * \endinternal
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
#include "global_data.h"

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
  return static_cast<Physframe *>(mem);
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
    return nullptr;

  Physframe *pf = Physframe::alloc(cxx::int_value<Page>(key_end));

  if (EXPECT_FALSE(!pf))
    return nullptr;

  void *m = alloc();
  if (EXPECT_FALSE(!m))
    {
      Physframe::free(pf, cxx::int_value<Page>(key_end), owner_id);
      return nullptr;
    }

  quota.release();
  return new (m) Treemap(key_end, owner_id, page_offset, page_shift, shifts + 1,
                         shifts_num - 1, pf);
}

static DEFINE_GLOBAL
Global_data<Kmem_slab_t<Treemap>> _treemap_allocator("Treemap");

static
void *
Treemap::alloc()
{ return _treemap_allocator->alloc(); }

static
void
Treemap::free(void *e)
{ _treemap_allocator->free(e); }


PUBLIC inline
void
Treemap::operator delete (Treemap *t, std::destroying_delete_t)
{
  Space *id = t->_owner_id;
  auto end = t->_key_end;

  t->~Treemap();
  free(t);
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

/**
 * Lookup a mapping, starting from a mapping in locked #Physframe and optionally
 * return whether the returned mapping is in a subtree of `m`.
 *
 * \param[in]    f            Locked #Physframe containing the mapping to start
 *                            the lookup from.
 * \param[in]    m            Mapping to start the lookup from.
 * \param[in]    spc          Virtual address space in which the mapping was entered.
 * \param[in]    key          Physical key of the mapped page frame.
 * \param[in]    va           Virtual address of the mapping. Required to select
 *                            the correct mapping if the address space contains
 *                            multiple mappings of the same physical address at
 *                            different virtual addresses.
 * \param[out]   res          If found, iterator pointing to found Mapping,
 *                            pointer to the containing #Treemap, and pointer to
 *                            the containing #Physframe.
 * \param[in,out] root_depth  If not `nullptr`, must point to depth of `m`. Is
 *                            set to `-3` when the returned Mapping is not in a
 *                            subtree of `m`. Only considered if also
 *                            `current_depth != nullptr`.
 * \param[out] current_depth  If not `nullptr`, outputs depth of returned
 *                            Mapping.
 * \return `m` or a successor of `m` where `res` was found, or end() if not
 *         found.
 */
PRIVATE inline
Mapping_tree::Iterator
Treemap::_lookup(Physframe *f, Mapping_tree::Iterator m,
                 Space const *spc, Pcnt key, Pfn va,
                 Frame *res, int *root_depth = nullptr, int *current_depth = nullptr)
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

/**
 * \see Mapdb::lookup()
 */
PUBLIC
bool
Treemap::lookup(Pcnt key, Space const *search_space, Pfn search_va,
                Frame *res)
{
  // get and lock the tree.
  assert (trunc_to_page(key) < _key_end);
  Physframe *f = tree(trunc_to_page(key)); // returns locked frame

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

/**
 * \see Mapdb::lookup_src_dst()
 */
PUBLIC inline NEEDS[Treemap::_lookup, "assert_opt.h"]
int
Treemap::lookup_src_dst(Space const *src, Pcnt src_key, Pfn src_va,
                        Space const *dst, Pcnt dst_key, Pfn dst_va,
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
          int r = sub->lookup_src_dst(src, src_subkey, src_va,
                                      dst, dst_subkey, dst_va,
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

      if (r_depth == c_depth && dst_frame->m->depth() == f->min_depth())
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
          if (r_depth < static_cast<int>(f->min_depth()) - 1)
            return 1; // in another subtree -> unmap + map

          return 2; // in the same subtree -> unmap + no map
        }

      // smaller mapping in submap found...
      if (r_depth == c_depth)
        {
          // src is in subtree of dst -> unmap dst and nothing to map then
          src_frame->clear();
          return 2; // unmap + no map
        }
      // src is in a sibling subtree -> unmap + map
      // needs to be kept locked
      return 1;
    }
}

/**
 * \see Mapdb::insert()
 */
PUBLIC
Mapping *
Treemap::insert(Physframe* frame, Mapping_tree::Iterator const &parent,
                Space *parent_space, Pfn parent_va,
                Space *space, Pfn va, Pcnt phys, Pcnt size)
{
  using Iterator = Mapping_tree::Iterator;

  if (page_size() == size)  // normal mapping
    {
      Iterator free = frame->tree()->allocate(Mapping_tree::quota(space),
                                              parent);
      if (EXPECT_FALSE(!*free))
        return nullptr;

      free->set_space(space);
      set_vaddr(*free, va);
      return *free;
    }

  // Inserting subpage mapping.  See if we can find a submap.  If so,
  // we don't have to allocate a new Mapping entry.
  Treemap* submap = frame->find_submap(parent);

  if (! submap)  // Need allocation of new entry for submap
    {
      // first check quota! In case of a new submap the parent pays for
      // the node...
      Ram_quota *payer = Mapping_tree::quota(parent_space);

      Iterator free = frame->tree()->allocate_submap(payer, parent);
      if (EXPECT_FALSE(!*free))
        return nullptr;

      assert (_sub_shifts_num > 0);

      submap = Treemap::create(_page_shift, parent_space, parent_va,
                               _sub_shifts, _sub_shifts_num);
      if (! submap)
        {
          // free the mapping got with allocate
          frame->free_mapping(payer, free);
          return nullptr;
        }

      free->set_submap(submap);
    }

  Pcnt subframe_offset = cxx::mask_lsb(cxx::get_lsb(phys, _page_shift),
                                       submap->page_shift());
  Physframe* subframe = submap->tree(submap->trunc_to_page(subframe_offset));
  if (! subframe)
    return nullptr;

  Mapping* ret = submap->insert(subframe, subframe->insertion_head(),
                                parent_space, parent_va + subframe_offset,
                                space, va, phys, size);

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

/**
 * \see Mapdb::flush()
 */
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

/**
 * \see Mapdb::grant()
 */
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

/**
 * Iterate over descendant mappings of a mapping.
 *
 * \param[in] f         The root of the subtree that is iterated over.
 * \param[in] va_begin  Begin of virtual address range of mappings to consider.
 * \param[in] va_end    End of virtual address range (exclusive) of mappings to
 *                      consider.
 * \param[in] func      A callable taking as arguments a Mapping_tree::Iterator
 *                      and a Mapdb::Order.
 *
 * \pre `f.frame->lock.test() == Locked`
 *
 * Iterate over all descendant mappings of `f` within the physical address range
 * corresponding to the virtual address range from `va_begin` to `va_end` for
 * `f`. For each of those mappings, `func` is called with a pointer to the
 * mapping and the size of the corresponding frame.
 */
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

 /**
  * Constructor.
  *
  * \param owner              The root of all mapping trees in this #Mapdb.
  * \param parent_page_shift  Log₂ size of physical address space managed by
  *                           this #Mapdb.
  * \param page_shifts        Array of log₂ sizes of allowed subdivisions of the
  *                           physical address space.
  * \param page_shifts_num    Length of `page_shifts`.
  *
  * \pre `parent_page_shift` > `page_shifts[0]` > `page_shifts[1]` > …
  *      > `page_shifts[page_shifts_num - 1]`
  *
  * \attention The entries of `page_shifts` must not be changed as long as this
  *            constructed instance of #Mapdb exists.
  */
PUBLIC inline NEEDS[<cassert>]
Mapdb::Mapdb(Space *owner, Order parent_page_shift, size_t const *page_shifts,
             unsigned page_shifts_num)
: _treemap(Treemap::create(parent_page_shift, owner,
                           Pfn(0), page_shifts, page_shifts_num))
{
  // assert (boot_time);
  assert (_treemap.get());
} // Mapdb()

/**
 * Insert a new mapping with the given parent.
 *
 * \param[in] frame  The parent of the new mapping.
 * \param[in] space  The target space and owner of the new mapping
 * \param[in] va     The virtual address of the mapping within `space`.
 * \param[in] phys   Start of the physical address range being mapped.
 * \param[in] size   The length of the mapped range. Must be one of the lengths
 *                   the mapping database was initialized with and at most as
 *                   large as the mapped range of `frame`.
 *
 * \retval nullptr    Insertion failed. Possible causes:
 *                    - out of quota
 *                    - out of memory
 *                    - insertion would exceed maximal depth of mapping tree
 * \retval otherwise  Pointer to the new #Mapping.
 *
 * \pre `frame.frame->lock.test() == Locked`
 * \pre `size` must be one of `frame.treemap->page_size()` or the corresponding
 *       size for an element in `frame.treemap->_sub_shifts`.
 *
 * Insert a new child mapping for `frame`. The insertion takes quota from
 * `space` and potentially also from the owner of `frame`.
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
 * Lookup a mapping and lock the corresponding #Physframe/#Mapping_tree.
 *
 * \param[in]  space  Virtual address space in which the mapping was entered.
 * \param[in]  va     Virtual address of the mapping. Required to select the
 *                    correct mapping if the address space contains multiple
 *                    mappings of the same physical address at different virtual
 *                    addresses.
 * \param[in]  phys   Physical address of the mapped page frame.
 * \param[out] res    If found, iterator pointing to found Mapping, pointer to
 *                    the containing #Treemap, and pointer to the containing
 *                    #Physframe. If `space` is Sigma0, then the iterator points
 *                    to Mapping_tree::end().
 *
 * \retval true   The mapping was found.
 * \retval false  The mapping was not found.
 *
 * \post If the mappings was found, then `res->frame->lock.test() == Locked`.
 *
 * Lookup the mapping that maps the physical address `phys` to the virtual
 * address `va` in `space`. If the mapping exists, the corresponding
 * #Physframe/#Mapping_tree is locked. Eventually the lock must be released with
 * one of the clearing methods of `res` (cf. Frame::clear() etc.).
 */
PUBLIC inline
bool
Mapdb::lookup(Space const *space, Pfn va, Pfn phys,
              Frame *res)
{
  return _treemap->lookup(phys - Pfn(0), space, va, res);
}

/**
 * Recursively delete mappings from a tree.
 *
 * \param[in] f         The root of the subtree to delete.
 * \param[in] mask      Indicates if `f` itself shall also be deleted (see
 *                      L4_map_mask::self_unmap()).
 * \param[in] va_start  Begin of virtual address range of mappings to delete.
 * \param[in] va_end    End of virtual address range (exclusive) of mappings to
 *                      delete.
 *
 * \pre `f.frame->lock.test() == Locked`
 *
 * Delete all descendant mappings of `f` within the physical address range
 * corresponding to the virtual address range from `va_start` to `va_end` for
 * `f`. The deletion does not split mappings, i.e. partially overlapping
 * mappings are deleted entirely.
 *
 * If `mask.self_unmap()`, then also the mapping `f` is deleted. Because of the
 * "no split" rule, the entire mapping `f` gets deleted, and thus also all
 * descendants are deleted regardless of `va_start` and `va_end`.
 *
 * The quota of all the deleted mappings is freed.
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

/**
 * Change ownership of a mapping.
 *
 * \param[in] f          Mapping to be modified.
 * \param[in] new_space  New owner.
 * \param[in] va         Virtual address of the mapping in the new address
 *                       space.
 *
 * \retval true   Success.
 * \retval false  Failed. New owner is out of quota.
 *
 * \pre `f.frame->lock.test() == Locked`
 *
 * Transfer the mapping `f` to the new owner `new_space`. The child mappings of
 * `f` are kept, i.e., they remain children of `f` despite the new owner. The
 * necessary quota for the mapping is freed for the old owner and taken from the
 * new owner. The operation fails if the new owner is out of quota.
 */
PUBLIC inline
bool
Mapdb::grant(Frame const &f, Space *new_space,
             Pfn va)
{
  return f.treemap->grant(f.frame, f.m, new_space, va);
}

/**
 * Lookup two mappings, respectively lock the corresponding
 * #Physframe/#Mapping_tree, and return the relation of the mappings to each
 * other, which indicates what the caller needs to do when mapping from src to
 * dst (see the description of the return values).
 *
 * \param[in]  src        Virtual address space in which the src mapping was
 *                        entered.
 * \param[in]  src_phys   Physical address of the mapped page frame for src.
 * \param[in]  src_va     Virtual address of the src mapping.
 * \param[in]  dst        Virtual address space in which the dst mapping was
 *                        entered.
 * \param[in]  dst_phys   Physical address of the mapped page frame for dst.
 * \param[in]  dst_va     Virtual address of the dst mapping.
 * \param[out] src_frame  Returns the src mapping if found.
 *                        Initially, `src_frame->frame` must be `nullptr`.
 * \param[out] dst_frame  Returns the dst mapping if found.
 *                        Initially, `dst_frame->frame` must be `nullptr`.
 *
 * \retval -1  Dst mapping, src mapping, or both not found.
 * \retval  0  Dst is a child mapping of src.
 *             When mapping from src to dst, dst can be reused/upgraded.
 * \retval  1  Dst and src are found and none of the other cases applies.
 *             When mapping from src to dst, dst must be unmapped before src can
 *             be mapped to dst.
 * \retval  2  Dst is equal to or an ancestor of src.
 *             When mapping from src to dst, dst must be unmapped inducing also
 *             unmapping of src.
 *
 * \pre `src_frame->frame == nullptr && dst_frame->frame == nullptr`
 * \post If both mappings are found, then
 *       `src_frame->frame->lock.test() == Locked` and
 *       `dst_frame->frame->lock.test() == Locked`.
 *       With the exception of "Dst is equal to or an ancestor of src", then
 *       only the `dst_frame` is set and locked.
 *
 * Simultaneously lookup two mappings, one called src, the other called dst. The
 * return value indicates if both mappings are found, and, if so, how they
 * relate to each other. If both mappings are found, the respectively
 * corresponding #Physframe/#Mapping_tree is locked; otherwise, nothing is
 * locked.
 *
 * \see lookup()
 */
PUBLIC inline
int
Mapdb::lookup_src_dst(Space const *src, Pfn src_phys, Pfn src_va,
                      Space const *dst, Pfn dst_phys, Pfn dst_va,
                      Frame *src_frame, Frame *dst_frame)
{
  return _treemap->lookup_src_dst(src, src_phys - Pfn(0), src_va,
                                  dst, dst_phys - Pfn(0), dst_va,
                                  src_frame, dst_frame);
}

//--------------------------------------------------------------------
IMPLEMENTATION[!mapdb]:

PUBLIC
Mapdb::Mapdb(Space *, Order, size_t const *, unsigned)
{}

PUBLIC inline
Mapping *
Mapdb::insert(Frame const &, Space *, Pfn, Pfn, Pcnt)
{
  return reinterpret_cast<Mapping *>(1);
}

PUBLIC inline
bool
Mapdb::lookup(Space const *, Pfn, Pfn, Frame *)
{
  return true;
}

PUBLIC static inline
void
Mapdb::flush(Frame const &, L4_map_mask, Pfn, Pfn)
{}

PUBLIC inline
bool
Mapdb::grant(Frame const &, Space *, Pfn)
{
  return true;
}

PUBLIC inline
int
Mapdb::lookup_src_dst(Space const *, Pfn, Pfn,
                      Space const *, Pfn, Pfn,
                      Frame *, Frame *)
{
  return 0;
}

PUBLIC inline
bool
Mapdb::valid_address(Pfn)
{
  return true;
}

PUBLIC template<typename F> static inline
void
Mapdb::foreach_mapping(Frame const &, Mapdb::Pfn, Mapdb::Pfn, F &&)
{}
