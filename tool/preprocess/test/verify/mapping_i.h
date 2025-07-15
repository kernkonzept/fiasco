// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef mapping_i_h
#define mapping_i_h
#include "mapping.h"
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

//
// INTERFACE internal declaration follows
//

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
  
  unsigned 
  number_of_entries() const;
  
#line 186 "mapping.cpp"
  mapping_t *
  mappings();
} __attribute__((packed));
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
#line 392 "mapping.cpp"

struct physframe_data {
  mapping_tree_t *tree;
  helping_lock_t lock;
};

#endif // mapping_i_h
