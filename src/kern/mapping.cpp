INTERFACE:

#include "types.h"
#include "mapdb_types.h"
#include "cxx/slist"

class Space;
class Treemap;

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
class Mapping : public cxx::S_list_item
{
  template<typename T,
           typename INTERNAL = Mword,
           unsigned long PREFIX = 0>
  struct Kptr
  {
    INTERNAL _v;
    T *ptr() const
    { return _v ? reinterpret_cast<T*>(PREFIX | _v) : 0; }

    operator T * () const { return ptr(); }
    T &operator * () const { return *ptr(); }
    T *operator -> () const { return *ptr(); }

    Kptr() = default;
    Kptr(T *p) : _v(reinterpret_cast<unsigned long>(p)) {}
  };

  friend class Jdb_mapdb;
  friend class Mapping_tree;

  Kptr<Space> _space = nullptr;

  union {
    unsigned long _virt = 0;
    Treemap *_submap;
  };

  Mapping(Mapping const &) = delete;
  Mapping &operator = (Mapping const &) = delete;

public:
  Mapping() = default;
  typedef Mdb_types::Order Order;
  typedef Mdb_types::Pcnt Pcnt;
  typedef Mdb_types::Pfn Pfn;
  struct Page_t;
  typedef cxx::int_type_order<Address, Page_t, Order> Page;

  Space *space() const { return _space; }
  unsigned long depth() const { return _virt & 0xff; }
  bool has_max_depth() const { return depth() == 0xff; }
};


IMPLEMENTATION:

#include "config.h"

/** Set address space.
    @param space the address space into which the frame is mapped. 
 */
PUBLIC inline
void
Mapping::set_space(Space *space)
{
  // assert (space);
  _space = space;
}

/** Virtual address.
    @return the virtual address at which the frame is mapped.
 */
PUBLIC inline
Mapping::Page
Mapping::page() const
{
  return Page(_virt >> 8);
}

PUBLIC inline
Mapping::Pfn
Mapping::pfn(Mapping::Order order) const
{
  return Pfn(_virt >> 8) << order;
}

/** Set virtual address.
    @param address the virtual address at which the frame is mapped.
 */
PUBLIC inline
void
Mapping::set_page(Page address)
{
  _virt = (_virt & 0xff) | (cxx::int_value<Page>(address) << 8);
}

/** Set depth of mapping in mapping tree. */
PUBLIC inline
void
Mapping::set_depth(unsigned char depth)
{
  _virt = (_virt & ~0xffUL) | depth;
}

PUBLIC inline 
bool
Mapping::is_root() const
{
  return depth() == 0;
}

PUBLIC inline
Treemap *
Mapping::submap() const
{
  return _space ? 0 : _submap;
}

PUBLIC inline
void
Mapping::set_submap(Treemap *treemap)
{
  _submap = treemap;
  _space = 0;
}

// -------------------------------------------------------------
IMPLEMENTATION [debug]:

// this is all dummy code to make jdb_mapdb happy

PUBLIC inline
bool
Mapping::is_end_tag() const
{ return false; }

PUBLIC inline
bool
Mapping::unused() const
{ return false; }
