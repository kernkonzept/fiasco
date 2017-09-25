INTERFACE:

#include "types.h"
#include "mapdb_types.h"

class Space;
/** Represents one mapping in a mapping tree.
    Instances of Mapping ("mappings") work as an iterator over a
    mapping tree.  Mapping trees are never visible on the user level.
    Mappings are looked up directly in the mapping database (class
    Mapdb) using Mapdb::lookup().  When carrying out such a
    lookup, the mapping database locks the corresponding mapping tree
    before returning the mapping.  This mapping can the be used to
    iterate over the tree and to look up other mappings in the tree.
    The mapping tree is unlocked (and all mappings pointing into it
    become invalid) when Mapdb::free is called with any one of its
    mappings.
 */
class Mapping
{
  friend class Jdb_mapdb;
public:
  typedef Mdb_types::Order Order;
  typedef Mdb_types::Pcnt Pcnt;
  typedef Mdb_types::Pfn Pfn;
  struct Page_t;
  typedef cxx::int_type_order<Address, Page_t, Order> Page;

  enum Mapping_depth
  {
    Depth_root = 0, Depth_max = 252, 
    Depth_submap = 253, Depth_empty = 254, Depth_end = 255 
  };

  enum { Alignment = Mapping_entry::Alignment };

  // CREATORS
  Mapping(const Mapping&);	// this constructor is undefined.

  // DATA
  Mapping_entry _data;
  static_assert (__alignof__(Mapping_entry) == Alignment, "WRONG ALIGNMENT");
} __attribute__((packed, aligned(__alignof__(Mapping_entry))));


IMPLEMENTATION:

#include "config.h"

PUBLIC inline 
Mapping::Mapping()
{}

inline 
Mapping_entry *
Mapping::data()
{
  return &_data;
}

inline 
const Mapping_entry *
Mapping::data() const
{
  return &_data;
}

/** Address space.
    @return the address space into which the frame is mapped. 
 */
PUBLIC inline NEEDS [Mapping::data]
Space *
Mapping::space() const
{
  return data()->space();
}

/** Set address space.
    @param space the address space into which the frame is mapped. 
 */
PUBLIC inline NEEDS [Mapping::data]
void
Mapping::set_space(Space *space)
{
  data()->set_space(space);
}

/** Virtual address.
    @return the virtual address at which the frame is mapped.
 */
PUBLIC inline NEEDS[Mapping::data]
Mapping::Page
Mapping::page() const
{
  return Page(data()->data.address);
}

PUBLIC inline NEEDS[Mapping::data]
Mapping::Pfn
Mapping::pfn(Mapping::Order order) const
{
  return Pfn(data()->data.address) << order;
}

/** Set virtual address.
    @param address the virtual address at which the frame is mapped.
 */
PUBLIC inline NEEDS[Mapping::data]
void
Mapping::set_page(Page address)
{
  data()->data.address = cxx::int_value<Page>(address);
}

/** Depth of mapping in mapping tree. */
PUBLIC inline
unsigned 
Mapping::depth() const
{
  return data()->_depth;
}

/** Set depth of mapping in mapping tree. */
PUBLIC inline
void 
Mapping::set_depth(unsigned depth)
{
  data()->_depth = depth;
}

/** free entry?.
    @return true if this is unused.
 */
PUBLIC inline NEEDS [Mapping::data]
bool
Mapping::unused() const
{
  return depth() >= Depth_empty;
}

PUBLIC inline 
bool
Mapping::is_end_tag() const
{
  return depth() == Depth_end;
}

PUBLIC inline
Treemap *
Mapping::submap() const
{
  return (data()->_depth == Depth_submap) 
    ? data()->_submap
    : 0;
}

PUBLIC inline
void
Mapping::set_submap(Treemap *treemap)
{
  data()->_submap = treemap;
  data()->_depth = Depth_submap;
}

PUBLIC inline
void
Mapping::set_unused()
{
  data()->_depth = Depth_empty;
}

/** Parent.
    @return parent mapping of this mapping.
 */
PUBLIC Mapping *
Mapping::parent()
{
  if (depth() <= Depth_root)
    return 0;			// Sigma0 mappings don't have a parent.

  // Iterate over mapping entries of this tree backwards until we find
  // an entry with a depth smaller than ours.  (We assume here that
  // "special" depths (empty, end) are larger than Depth_max.)
  Mapping *m = this - 1;

  // NOTE: Depth_unused / Depth_submap are high, so no need to test
  // for them
  while (m->depth() >= depth())
    m--;

  return m;
}

