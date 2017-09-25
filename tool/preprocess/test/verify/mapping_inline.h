// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef mapping_inline_h
#define mapping_inline_h
#line 2 "mapping.cpp"

#include <flux/x86/types.h>	// for vm_offset_t, vm_size_t
#line 4 "mapping.cpp"
#include "space.h"		// for space_index_t

//
// INTERFACE definition follows 
//

#line 7 "mapping.cpp"

class mapping_tree_t;		// forward decls
#line 9 "mapping.cpp"
struct mapping_s;
#line 26 "mapping.cpp"

class kmem_slab_t;
#line 28 "mapping.cpp"
struct physframe_data;
#line 5 "mapping.cpp"

enum mapping_type_t { Map_mem = 0, Map_io };
#line 10 "mapping.cpp"

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

public:  
#line 224 "mapping.cpp"
  inline space_index_t 
  space();
  
#line 231 "mapping.cpp"
  inline vm_offset_t 
  vaddr();
  
#line 238 "mapping.cpp"
  inline vm_size_t 
  size();
  
#line 248 "mapping.cpp"
  inline mapping_type_t 
  type();
  
#line 283 "mapping.cpp"
  // 
  // more of class mapping_t
  // 
  
  mapping_t *
  parent();
  
#line 307 "mapping.cpp"
  mapping_t *
  next_iter();
  
#line 332 "mapping.cpp"
  mapping_t *
  next_child(mapping_t *parent);

private:  
#line 213 "mapping.cpp"
  inline mapping_t();
  
#line 217 "mapping.cpp"
  inline mapping_s *
  data();
  
#line 256 "mapping.cpp"
  inline bool
  unused();
  
#line 262 "mapping.cpp"
  mapping_tree_t *
  tree();
} __attribute__((packed));
#line 29 "mapping.cpp"

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

public:  
#line 399 "mapping.cpp"
  mapdb_t();
  
#line 450 "mapping.cpp"
  // insert a new mapping entry with the given values as child of
  // "parent" After locating the right place for the new entry, it will
  // be stored there (if this place is empty) or the following entries
  // moved by one entry.
  
  // We assume that there is at least one free entry at the end of the
  // array so that at least one insert() operation can succeed between a
  // lock()/free() pair of calls.  This is guaranteed by the free()
  // operation which allocates a larger tree if the current one becomes
  // to small.
  mapping_t *
  insert(mapping_t *parent,
  		space_t *space, 
  		vm_offset_t va, 
  		vm_size_t size,
  		mapping_type_t type);
  
#line 562 "mapping.cpp"
  mapping_t *
  lookup(space_t *space,
  		vm_offset_t va,
  		mapping_type_t type);
  
#line 603 "mapping.cpp"
  void 
  free(mapping_t* mapping_of_tree);
  
#line 720 "mapping.cpp"
  // Delete mappings from a tree.  This is easy to do: We just have to
  // iterate over the array encoding the tree.
  bool 
  flush(mapping_t *m, bool me_too);
  
#line 779 "mapping.cpp"
  void 
  grant(mapping_t *m, space_t *new_space, vm_offset_t va);
};

//
// IMPLEMENTATION of inline functions (and needed classes)
//

#line 191 "mapping.cpp"

// Define (otherwise private) stuff needed by public inline
// functions.

enum mapping_depth_t 
{
  Depth_sigma0_mapping = 0, Depth_max = 253, 
  Depth_empty = 254, Depth_end = 255 
};
#line 200 "mapping.cpp"

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

#line 215 "mapping.cpp"


inline mapping_s *
mapping_t::data()
{
  return reinterpret_cast<mapping_s*>(_data);
}

#line 222 "mapping.cpp"


inline space_index_t 
mapping_t::space()
{
  return space_index_t(data()->space);
}

#line 229 "mapping.cpp"


inline vm_offset_t 
mapping_t::vaddr()
{
  return (data()->address << PAGE_SHIFT);
}

#line 236 "mapping.cpp"


inline vm_size_t 
mapping_t::size()
{
  if ( data()->size ) 
    return SUPERPAGE_SIZE;
  else 
    return PAGE_SIZE;
}

#line 246 "mapping.cpp"


inline mapping_type_t 
mapping_t::type()
{
  // return data()->type;;
  return Map_mem;
}

#endif // mapping_inline_h
