INTERFACE:

#include <flux/x86/types.h>	// for vm_offset_t, vm_size_t
#include "space.h"		// for space_index_t

enum mapping_type_t { Map_mem = 0, Map_io };

class mapping_tree_t;		// forward decls
struct mapping_s;

// 
// class mapping_t
// 
class mapping_t 
{
  friend class mapdb_t;
  friend class mapping_tree_t;
  friend class jdb;

  // CREATORS
  mapping_t(const mapping_t&);	// this constructor is undefined.

  // DATA
  char _data[5];  
} __attribute__((packed));

class kmem_slab_t;
struct physframe_data;

// 
// class mapdb_t: The mapping database
// 
class mapdb_t
{
  friend class jdb;

public:
  enum { Size_factor = 4, 
	 Size_id_max = 8 /* can be up to 15 (4 bits) */ };

private:
  // DATA
  physframe_data *physframe;
  kmem_slab_t *allocator_for_treesize[Size_id_max + 1];
};

IMPLEMENTATION:

// The mapping database.

// This implementation encodes mapping trees in very compact arrays of
// fixed sizes, prefixed by a tree header (mapping_tree_t).  Array
// sizes can vary from 4 mappings to 4<<15 mappings.  For each size,
// we set up a slab allocator.  To grow or shrink the size of an
// array, we have to allocate a larger or smaller tree from the
// corresponding allocator and then copy the array elements.
// 
// The array elements (mapping_t) contain a tree depth element.  This
// depth and the relative position in the array is all information we
// need to derive tree structure information.  Here is an example:
// 
// array
// element   depth
// number    value    comment
// --------------------------
// 0         0        Sigma0 mapping
// 1         1        child of element #0 with depth 0
// 2         2        child of element #1 with depth 1
// 3         2        child of element #1 with depth 1
// 4         3        child of element #3 with depth 2
// 5         2        child of element #1 with depth 1
// 6         3        child of element #5 with depth 2
// 7         1        child of element #0 with depth 0
// 
// This array is a pre-order encoding of the following tree:
// 
//                   0
// 	          /     \ 
//               1       7
//            /  |  \                   
//           2   3   5
//               |   |
//        	 4   6
       	       	   
// IDEAS for enhancing this implementation: 

// We often have to find a tree header corresponding to a mapping.
// Currently, we do this by iterating backwards over the array
// containing the mappings until we find the Sigma0 mapping, from
// whose address we can compute the address of the tree header.  If
// this becomes a problem, we could add one more byte to the mappings
// with a hint (negative array offset) where to find the sigma0
// mapping.  (If the hint value overflows, just iterate using the hint
// value of the mapping we find with the first hint value.)  Another
// idea (from Adam) would be to just look up the tree header by using
// the physical address from the page-table lookup, but we would need
// to change the interface of the mapping database for that (pass in
// the physical address at all times), or we would have to include the
// physical address (or just the address of the tree header) in the
// mapdb_t-user-visible mapping_t (which could be different from the
// internal tree representation).  (XXX: Implementing one of these
// ideas is probably worthwile doing!)

// Instead of copying whole trees around when they grow or shrink a
// lot, or copying parts of trees when inserting an element, we could
// give up the array representation and add a "next" pointer to the
// elements -- that is, keep the tree of mappings in a
// pre-order-encoded singly-linked list (credits to: Christan Szmajda
// and Adam Wiggins).  24 bits would probably be enough to encode that
// pointer.  Disadvantages: Mapping entries would be larger, and the
// cache-friendly space-locality of tree entries would be lost.

// The current handling of superpages sucks rocks both in this module
// and in our user, ipc_map.cc.  We could support multiple page sizes
// by not using a physframe[] array only for the largest page size.
// (Entries of that array point to the top-level mappings -- sigma0
// mappings.)  Mapping-tree entries would then either be regular
// mappings or pointers to an array of mappings of the next-smaller
// size. (credits: Christan Szmajda)
//
//        physframe[]
//        -------------------------------
//     	  | | | | | | | | | | | | | | | | array of ptr to 4M mapping_tree_t's
//        ---|---------------------------
//           |
//           v a mapping_tree_t
//           ---------------
//           |             | tree header
//           |-------------|
//           |             | mapping_t *or* ptr to array of ptr to 4K trees
//           |             | e.g.
//           |      ----------------|
//           |             |        v array of ptr to 4M mapping_tree_t's
//           ---------------        -------------------------------
//                                  | | | | | | | | | | | | | | | |
//                                  ---|---------------------------
//                                     |
//                                     v a mapping_tree_t
//                             	       ---------------
//                                     |             | tree header
//                                     |-------------|
//                                     |             | mapping_t
//                                     |             |
//                                     |             |
//                                     |             |
//                                     ---------------
//-

#include <assert.h>
#include <string.h>

#ifndef offsetof		// should be defined in stddef.h, but isn't
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

// For implementation of mapping_t functions, we need mapping_tree_t.

// 
// class mapping_tree_t
// 
class mapping_tree_t
{
  friend class mapping_t;
  friend class mapdb_t;
  friend class jdb;

  // DATA
  unsigned _count: 16;
  unsigned _size_id: 4;
  unsigned _empty_count: 11;
  unsigned _unused: 1;		// make this 32 bits to avoid a compiler bug

  mapping_t _mappings[0] __attribute__((packed));

} __attribute__((packed));

// public routines with inline implementations
inline
unsigned 
mapping_tree_t::number_of_entries() const
{
  return mapdb_t::Size_factor << _size_id;
}

inline
mapping_t *
mapping_tree_t::mappings()
{
  return & _mappings[0];
}

// Define (otherwise private) stuff needed by public inline
// functions.

enum mapping_depth_t 
{
  Depth_sigma0_mapping = 0, Depth_max = 253, 
  Depth_empty = 254, Depth_end = 255 
};

struct mapping_s
{
  unsigned space:11;
  unsigned size:1;
  unsigned address:20;
  mapping_depth_t depth:8;
  unsigned do_not_touch: 24; // make this 64 bits, and make sure the
                             // compiler never touches memory after the 
                             // real data
};

inline 
mapping_t::mapping_t()
{}

inline 
mapping_s *
mapping_t::data()
{
  return reinterpret_cast<mapping_s*>(_data);
}

PUBLIC inline NEEDS[mapping_depth_t, mapping_s, mapping_t::data]
space_index_t 
mapping_t::space()
{
  return space_index_t(data()->space);
}

PUBLIC inline NEEDS[mapping_depth_t, mapping_s, mapping_t::data]
vm_offset_t 
mapping_t::vaddr()
{
  return (data()->address << PAGE_SHIFT);
}

PUBLIC inline NEEDS[mapping_depth_t, mapping_s, mapping_t::data]
vm_size_t 
mapping_t::size()
{
  if ( data()->size ) 
    return SUPERPAGE_SIZE;
  else 
    return PAGE_SIZE;
}

PUBLIC inline 
mapping_type_t 
mapping_t::type()
{
  // return data()->type;;
  return Map_mem;
}

inline NEEDS[mapping_depth_t, mapping_s, mapping_t::data]
bool
mapping_t::unused()
{
  return (data()->depth > Depth_max);
}

mapping_tree_t *
mapping_t::tree()
{
  mapping_t *m = this;

  while (m->data()->depth > Depth_sigma0_mapping)
    {
      // jump in bigger steps if this is not a free mapping
      if (! m->unused())
	{
	  m -= m->data()->depth;
	  continue;
	}

      m--;
    }

  return reinterpret_cast<mapping_tree_t *>
    (reinterpret_cast<char *>(m) - offsetof(mapping_tree_t, _mappings));
}

// 
// more of class mapping_t
// 

PUBLIC mapping_t *
mapping_t::parent()
{
  if (data()->depth <= Depth_sigma0_mapping)
    {
      // Sigma0 mappings don't have a parent.
      return 0;
    }

  // Iterate over mapping entries of this tree backwards until we find
  // an entry with a depth smaller than ours.  (We assume here that
  // "special" depths (empty, end) are larger than Depth_max.)
  mapping_t *m = this - 1;

  while (m->data()->depth >= data()->depth)
    m--;

  return m;
}

PUBLIC mapping_t *
mapping_t::next_iter()
{
  mapping_tree_t *t = tree();
  mapping_t *last_in_tree = t->mappings() + t->number_of_entries() - 1;

  mapping_t *m = this;

  // Look for a valid element in the tree structure.
  while (m != last_in_tree)
    {
      m++;
      if (! m->unused())
	return m;		// Found a valid entry!

      // Don't iterate to end of tree if this is the last tree entry.
      if (m->data()->depth == Depth_end)
	return 0;
    }

  // Couldn't find a non-empty element behind ourselves in the tree
  // structure.
  return 0;
}

PUBLIC mapping_t *
mapping_t::next_child(mapping_t *parent)
{
  // Find the next valid entry in the tree structure.
  mapping_t *m = next_iter();

  // If we didn't find an entry, or if the entry cannot be a child of
  // "parent", return 0
  if (m == 0
      || m->data()->depth <= parent->data()->depth)
    return 0;

  return m;			// Found!
}

// helpers

// 
// class mapping_tree_t
// 

// This function copies the elements of mapping tree src to mapping
// tree dst, ignoring empty elements (that is, compressing the
// source tree.  In-place compression is supported.
PUBLIC static void 
mapping_tree_t::copy_compact_tree(mapping_tree_t *dst, mapping_tree_t *src)
{
  dst->_count = 0;
  dst->_empty_count = 0;
  
  mapping_t *d = dst->mappings();     
  
  for (mapping_t *s = src->mappings();
       s < src->mappings() + src->number_of_entries();
       s++)
    {
      if (s->unused())		// entry free
	{
	  if (s->data()->depth == Depth_end)
	    break;
	  continue;
	}
      
      *d++ = *s;
      dst->_count += 1;
    }
  
  assert (dst->_count == src->_count);
  assert (d < dst->mappings() + dst->number_of_entries());
  
  d->data()->depth = Depth_end;
  dst->mappings()[dst->number_of_entries() - 1].data()->depth = Depth_end;
} // copy_compact_tree()

// 
// class mapdb
// 

#include "lock.h"
#include "kmem_slab.h"

struct physframe_data {
  mapping_tree_t *tree;
  helping_lock_t lock;
};

PUBLIC
mapdb_t::mapdb_t()
{
  vm_offset_t page_number = kmem::info()->main_memory.high / PAGE_SIZE + 1;
  
  // allocate physframe array
  check ( physframe = reinterpret_cast<physframe_data *>
	  (kmem::alloc(page_number * sizeof(physframe_data))) );

  memset(physframe, 0, page_number * sizeof(physframe_data));

  // create a slab for each mapping tree size
  for (int slab_number = 0;
       slab_number <= Size_id_max;
       slab_number++ )
    {
      unsigned elem_size = 
	(Size_factor << slab_number) * sizeof(mapping_t) 
	+ sizeof(mapping_tree_t);

      allocator_for_treesize[slab_number] =
	new kmem_slab_t(((PAGE_SIZE / elem_size) < 40
			 ? 8*PAGE_SIZE : PAGE_SIZE),
			elem_size, 1);
    }

  // create a sigma0 mapping for all physical pages
  for (unsigned page_id = 0; page_id < page_number; page_id++)
    {
      mapping_tree_t *t;
      check( (t = physframe[page_id].tree
	      = reinterpret_cast<mapping_tree_t*>
	          (allocator_for_treesize[0]->alloc())) );
      
      t->_count = 1;		// 1 valid mapping
      t->_size_id = 0;		// size is equal to Size_factor << 0
      t->_empty_count = 0;	// no gaps in tree representation

      t->mappings()[0].data()->depth = Depth_sigma0_mapping;
      t->mappings()[0].data()->address = page_id;
      t->mappings()[0].data()->space = config::sigma0_taskno;
      t->mappings()[0].data()->size = 0;

      t->mappings()[1].data()->depth = Depth_end;

      // We also always set the end tag on last entry so that we can
      // check whether it has been overwritten.
      t->mappings()[t->number_of_entries() - 1].data()->depth = Depth_end;
    }    
} // mapdb_t()


// insert a new mapping entry with the given values as child of
// "parent" After locating the right place for the new entry, it will
// be stored there (if this place is empty) or the following entries
// moved by one entry.

// We assume that there is at least one free entry at the end of the
// array so that at least one insert() operation can succeed between a
// lock()/free() pair of calls.  This is guaranteed by the free()
// operation which allocates a larger tree if the current one becomes
// to small.
PUBLIC mapping_t *
mapdb_t::insert(mapping_t *parent,
		space_t *space, 
		vm_offset_t va, 
		vm_size_t size,
		mapping_type_t type)
{
  assert(type == Map_mem);	// we don't yet support Map_io

  mapping_tree_t *t = parent->tree();

  // We cannot continue if the last array entry is not free.  This
  // only happens if an earlier call to free() with this mapping tree
  // couldn't allocate a bigger array.  In this case, signal an
  // out-of-memory condition.
  if (! t->mappings()[t->number_of_entries() - 1].unused())
    return 0;

  // If the parent mapping already has the maximum depth, we cannot
  // insert a child.
  if (parent->data()->depth == Depth_max)
    return 0;

  mapping_t *insert = 0;

  bool 
    found_free_entry = false,
    need_to_move = false;

  mapping_t temp;

  // Find a free entry in the array encoding the tree, and find the
  // insertion point for the new entry.  These two entries might not
  // be equivalent, so we may need to move entries backwards in the
  // array.  In this implementation, we move entries as we traverse
  // the array, instead of doing one big memmove
  for (mapping_t *m = parent + 1;
       m < t->mappings() + t->number_of_entries();
       m++)
    {
      if (m->unused())
	{
	  // We found a free entry in the tree -- allocate it.
	  found_free_entry = true;
	  t->_count += 1;

	  // Did we find an empty element != the end tag?
	  if (m->data()->depth == Depth_empty)
	    {
	      t->_empty_count -= 1;
	    }
	  // Else we have found the end tag.  If there is another
	  // array entry left, apply a new end tag to the array
	  else if (m + 1 < t->mappings() + t->number_of_entries())
	    {
	      (m + 1)->data()->depth = Depth_end;
	    }

	  // if we haven't identified a place for inserting the new
	  // element, this is it.
	  if (! need_to_move)
	    insert = m;
	}
      else if (! need_to_move
	       && m->data()->depth <= parent->data()->depth)
	{
	  // we found a non-descendant of ourselves -- need to make
	  // space for new child
	  need_to_move = true;
	  insert = m;
	  temp = *insert;

	  continue;
	}

      if (need_to_move)
	{
	  // replace *m with temp (which used to be *(m - 1); but 
	  // *(m - 1) has since been overwritten), and load temp with 
	  // the old value of *m
	  mapping_t temp2;

	  temp2 = *m;
	  *m = temp;
	  temp = temp2;
	}

      if (found_free_entry)
	break;
    }

  assert(insert && found_free_entry);

  // found a place to insert new child.
  insert->data()->depth = mapping_depth_t(parent->data()->depth + 1);
  insert->data()->address = va >> PAGE_SHIFT;
  insert->data()->space = space->space();
  insert->data()->size = (size == SUPERPAGE_SIZE);

  return insert;
} // insert()

PUBLIC mapping_t *
mapdb_t::lookup(space_t *space,
		vm_offset_t va,
		mapping_type_t type)
{
  vm_offset_t phys = space->virt_to_phys(va);

  if (phys == 0xffffffff)
    return 0;

  mapping_tree_t *t;

  // get and lock the tree.
  // XXX For now, use a simple lock with helping.  Later
  // unify locking with our request scheme.
  physframe[phys >> PAGE_SHIFT].lock.lock();
  
  t = physframe[phys >> PAGE_SHIFT].tree;
  assert(t);

  mapping_t *m;

  for (m = t->mappings();
       m->data()->depth != Depth_end
	 && m < t->mappings() + t->number_of_entries();
       m++)
    {
      if (m->data()->space == space->space()
	  && m->data()->address == va >> PAGE_SHIFT)
	{
	  // found!
	  return m;
	}
    }

  // not found -- unlock tree
  physframe[phys >> PAGE_SHIFT].lock.clear();

  return 0;
} // lookup()

PUBLIC void 
mapdb_t::free(mapping_t* mapping_of_tree)
{
  mapping_tree_t *t = mapping_of_tree->tree();

  // We assume that the zeroth mapping of the tree is a sigma0
  // mapping, that is, its virtual address == the page's physical
  // address.
  vm_offset_t phys_pno = t->mappings()[0].data()->address;

  // We are the owner of the tree lock.
  assert(physframe[phys_pno].lock.lock_owner() == current());

  // Before we unlock the tree, we need to make sure that there is
  // room for at least one new mapping.  In particular, this means
  // that the last entry of the array encoding the tree must be free.

  // (1) When we use up less than a quarter of all entries of the
  // array encoding the tree, copy to a smaller tree.  Otherwise, (2)
  // if the last entry is free, do nothing.  Otherwise, (3) if less
  // than 3/4 of the entries are used, compress the tree.  Otherwise,
  // (4) copy to a larger tree.

  bool maybe_out_of_memory = false;

  do // (this is not actually a loop, just a block we can "break" out of)
    {
      // (1) Do we need to allocate a smaller tree?
      if (t->_size_id > 0	// must not be smallest size
	  && (t->_count << 2) < t->number_of_entries())
	{
	  mapping_tree_t *new_t = 
	    reinterpret_cast<mapping_tree_t *>
	      (allocator_for_treesize[t->_size_id - 1]->alloc());

	  if (new_t)
	    {
	      // XXX should be asserted by allocator:
	      new_t->_size_id = t->_size_id - 1; 
	      new_t->mappings()[new_t->number_of_entries() - 1].data()->depth 
		= Depth_end;
	      
	      mapping_tree_t::copy_compact_tree(new_t, t);
	      
	      // Register new tree.
	      physframe[phys_pno].tree = new_t;
	      
	      allocator_for_treesize[t->_size_id]->free(t);
	      t = new_t;
	      
	      break;
	    }
	}

      // (2) Is last entry is free?
      if (t->mappings()[t->number_of_entries() - 1].unused())
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

	  mapping_tree_t::copy_compact_tree(t, t); // in-place compression

	  break;
	}

      // (4) OK, allocate a bigger array.

      mapping_tree_t *new_t = 
	reinterpret_cast<mapping_tree_t*>
	  (allocator_for_treesize[t->_size_id + 1]->alloc());

      if (new_t)
	{
	  // XXX should be asserted by allocator:
	  new_t->_size_id = t->_size_id + 1; 
	  new_t->mappings()[new_t->number_of_entries() - 1].data()->depth 
	    = Depth_end;

	  mapping_tree_t::copy_compact_tree(new_t, t);

	  // Register new tree. 
	  physframe[phys_pno].tree = new_t;
	  
	  allocator_for_treesize[t->_size_id]->free(t);
	  t = new_t;
	}
      else
	{
	  // out of memory -- just do tree compression and hope that helps.
	  maybe_out_of_memory = true;

	  mapping_tree_t::copy_compact_tree(t, t); // in-place compression
	}
    } 
  while (false);

  // The last entry of the tree should now be free -- exept if we're
  // out of memory.
  assert(t->mappings()[t->number_of_entries() - 1].unused()
	 || maybe_out_of_memory);

  // Unlock tree.
  physframe[phys_pno].lock.clear();
} // free()

// Delete mappings from a tree.  This is easy to do: We just have to
// iterate over the array encoding the tree.
PUBLIC bool 
mapdb_t::flush(mapping_t *m, bool me_too)
{
  mapping_tree_t *t = m->tree();
  mapping_t *start_of_deletions = m;
  unsigned m_depth = m->data()->depth;
  unsigned deleted = 0, empty_elems_passed = 0;

  if (me_too)
    {
      m->data()->depth = Depth_empty;
      t->_count -= 1;
      deleted++;
    }
  else
    start_of_deletions++;

  m++;

  for (;
       m < t->mappings() + t->number_of_entries()
	 && m->data()->depth != Depth_end;
       m++)
    {
      if (unsigned (m->data()->depth) <= m_depth)
	{
	  // Found another data element -- stop deleting.  Since we
	  // created holes in the tree representation, account for it.
	  t->_empty_count += deleted;

	  return true;
	}

      if (m->data()->depth == Depth_empty)
	{
	  empty_elems_passed++;
	  continue;
	}

      // Delete the element.
      m->data()->depth = Depth_empty;
      t->_count -= 1;
      deleted++;
    }

  // We deleted stuff at the end of the array -- move end tag
  if (start_of_deletions < t->mappings() + t->number_of_entries())
    {
      start_of_deletions->data()->depth = Depth_end;

      // also, reduce number of free entries
      t->_empty_count -= empty_elems_passed;
    }

  return true;
} // flush()

PUBLIC void 
mapdb_t::grant(mapping_t *m, space_t *new_space, vm_offset_t va)
{
  m->data()->space = new_space->space();
  m->data()->address = va >> PAGE_SHIFT;
} // grant()
