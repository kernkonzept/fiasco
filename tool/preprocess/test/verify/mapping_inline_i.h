// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef mapping_inline_i_h
#define mapping_inline_i_h
#line 148 "mapping.cpp"

#include <assert.h>
#line 150 "mapping.cpp"
#include <string.h>
#line 385 "mapping.cpp"

// 
// class mapdb
// 

#include "lock.h"
#line 391 "mapping.cpp"
#include "kmem_slab.h"
#line 155 "mapping.cpp"

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


public:  
#line 347 "mapping.cpp"
  // helpers
  
  // 
  // class mapping_tree_t
  // 
  
  // This function copies the elements of mapping tree src to mapping
  // tree dst, ignoring empty elements (that is, compressing the
  // source tree.  In-place compression is supported.
  static void 
  copy_compact_tree(mapping_tree_t *dst, mapping_tree_t *src);

private:  
#line 177 "mapping.cpp"
  // public routines with inline implementations
  
  inline unsigned 
  number_of_entries() const;
  
#line 186 "mapping.cpp"
  inline mapping_t *
  mappings();
} __attribute__((packed));
#line 392 "mapping.cpp"

struct physframe_data {
  mapping_tree_t *tree;
  helping_lock_t lock;
};

//
// IMPLEMENTATION of inline functions follows
//


#line 211 "mapping.cpp"


inline mapping_t::mapping_t()
{}

#line 254 "mapping.cpp"


inline bool
mapping_t::unused()
{
  return (data()->depth > Depth_max);
}

#line 176 "mapping.cpp"

// public routines with inline implementations

inline unsigned 
mapping_tree_t::number_of_entries() const
{
  return mapdb_t::Size_factor << _size_id;
}

#line 184 "mapping.cpp"


inline mapping_t *
mapping_tree_t::mappings()
{
  return & _mappings[0];
}

#endif // mapping_inline_i_h
