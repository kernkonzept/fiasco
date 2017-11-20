INTERFACE:

#include <cxx/cxx_int>
#include <cxx/hlist>

#include "config.h"
#include "types.h"
#include "l4_types.h"
#include "l4_msg_item.h"
#include "template_math.h"

class Kobject_iface;
class Kobject_mapdb;
class Jdb_mapdb;

namespace Obj {

  struct Attr
  : cxx::int_type_base<unsigned char, Attr>,
    cxx::int_bit_ops<Attr>
  {
    Attr() = default;
    explicit Attr(unsigned char r)
    : cxx::int_type_base<unsigned char, Attr>(r) {}

    Attr(L4_fpage::Rights r, unsigned char extra)
    : cxx::int_type_base<unsigned char, Attr>(cxx::int_value<L4_fpage::Rights>(r) | extra)
    {}


    L4_fpage::Rights rights() const { return L4_fpage::Rights(_v & 0x0f); }
    unsigned char extra() const { return _v & 0xf0; }

    static Attr Full() { return Attr(0xff); }

    /// when cap R right is missing the cap cannot be mapped
    constexpr bool empty() const
    { return (_v & 0x4) == 0; }
  };

  class Capability
  {
  private:
    Mword _obj;

  public:
    Capability() {}
    explicit Capability(Mword v) : _obj(v) {}
    Kobject_iface *obj() const { return (Kobject_iface *)(_obj & ~3UL); }
    void set(Kobject_iface *obj, unsigned char rights)
    { _obj = Mword(obj) | rights; }
    bool valid() const { return _obj; }
    void invalidate() { _obj = 0; }
    unsigned char rights() const { return _obj & 3; }
    void add_rights(unsigned char r) { _obj |= r & 3; }
    void del_rights(L4_fpage::Rights r)
    { _obj &= ~(cxx::int_value<L4_fpage::Rights>(r) & 3); }

    bool operator == (Capability const &c) const { return _obj == c._obj; }
  };

  /**
   * Tn the case of a flat copy model for capabilities, we have some
   * extra mapping information directly colocated within the cap tables.
   */
  class Mapping : public cxx::H_list_item
  {
    friend class ::Kobject_mapdb;
    friend class ::Jdb_mapdb;

  public:
    typedef cxx::H_list<Mapping> List;

    enum Flag
    {
      Delete  = 0x08, // L4_fpage::Rights::CD()
      Ref_cnt = L4_msg_item::C_weak_ref,

      Initial_flags = Delete | Ref_cnt | L4_msg_item::C_ctl_rights
    };

  protected:
    Mword _flags : 8;
    Mword _pad   : 24;

  public:
    Mapping() : _flags(0) {}
    // fake this really badly
    Mapping *parent() { return this; }
    Mword delete_rights() const { return _flags & Delete; }
    Mword ref_cnt() const { return _flags & Ref_cnt; }

    void put_as_root() { _flags = Initial_flags; }
  };


  class Entry : public Capability, public Mapping
  {
  public:
    Entry() {}
    explicit Entry(Mword v) : Capability(v) {}

    Attr rights() const
    { return Attr(Capability::rights() | (_flags & ~3) | 0x4); }

    void set(Kobject_iface *obj, Attr rights)
    {
      Capability::set(obj, cxx::int_value<Attr>(rights) & 3);
      _flags = cxx::int_value<Attr>(rights) & 0xf8;
    }
    void add_rights(Attr r)
    {
      Capability::add_rights(cxx::int_value<Attr>(r) & 3);
      _flags |= (cxx::int_value<Attr>(r) & 0xf8);
    }

    void del_rights(L4_fpage::Rights r)
    {
      Capability::del_rights(r);
      _flags &= ~(cxx::int_value<L4_fpage::Rights>(r) & 0xf8);
    }
  };

  struct Cap_addr
  : public cxx::int_type_order_base<unsigned long, Cap_addr, Order, Cap_diff>
  {
  private:
    typedef cxx::int_type_order_base<unsigned long, Cap_addr, Order, Cap_diff> Base;

  public:
    mutable Entry *_c;
    Cap_addr() {}
    Cap_addr(unsigned long a, Entry *c) : Base(a), _c(c) {}
    explicit Cap_addr(Cap_index idx)
    : Base(cxx::int_value<Cap_index>(idx)), _c(0)
    {}

    explicit Cap_addr(unsigned long v) : Base(v), _c(0) {}

    operator Cap_index () const { return Cap_index(_v); }

    void set_entry(Entry *e) const { _c = e; }
  };

  inline void set_entry(Cap_addr const &ca, Entry *e)
  { ca.set_entry(e); }

  enum Insert_result
  {
    Insert_ok = 0,		///< Mapping was added successfully.
    Insert_warn_exists,		///< Mapping already existed
    Insert_warn_attrib_upgrade,	///< Mapping already existed, attribs upgrade
    Insert_err_nomem,		///< Couldn't alloc new page table
    Insert_err_exists		///< A mapping already exists at the target addr
  };

  enum
  {
    Caps_per_page_max = Config::PAGE_SIZE / sizeof(Obj::Entry),
    Caps_per_page_ld2 = Tl_math::Ld<Caps_per_page_max>::Res,
    Caps_per_page     = 1 << Caps_per_page_ld2,
  };
  static_assert(Caps_per_page == Caps_per_page_max, "hm, not a power of two caps on a single memory page");
}

// ------------------------------------------------------------------------
INTERFACE [debug]:

#include "dbg_page_info.h"
#include "warn.h"

class Space;

namespace Obj
{
  struct Cap_page_dbg_info
  {
    Address offset;
    Space *s;
  };

  inline void add_cap_page_dbg_info(void *p, Space *s, Address cap)
  {
    Dbg_page_info *info = new Dbg_page_info(Virt_addr((Address)p));

    if (EXPECT_FALSE(!info))
      {
        WARN("oom: could not allocate debug info fo page %p\n", p);
        return;
      }

    info->info<Cap_page_dbg_info>()->s = s;
    info->info<Cap_page_dbg_info>()->offset = (cap / Caps_per_page) * Caps_per_page;
    Dbg_page_info::table().insert(info);
  }

  inline void remove_cap_page_dbg_info(void *p)
  {
    Dbg_page_info *info = Dbg_page_info::table().remove(Virt_addr((Address)p));
    if (info)
      delete info;
    else
      WARN("could not find debug info for page %p\n", p);
  }
}


// ------------------------------------------------------------------------
INTERFACE [!debug]:

namespace Obj {
  static void add_cap_page_dbg_info(void *, void *, Address) {}
  static void remove_cap_page_dbg_info(void *) {}
}
