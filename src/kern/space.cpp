INTERFACE [!io]:

class Io_space;

//--------------------------------------------------------------------------
INTERFACE [io]:

#include "config.h"
#include "io_space.h"
#include "l4_types.h"

class Space;

/** Global definition of Io_space for map_util stuff. */
typedef Generic_io_space<Space> Io_space;

//--------------------------------------------------------------------------
INTERFACE:

#include "mem_space.h"
#include "member_offs.h"
#include "obj_space.h"
#include "spin_lock.h"
#include "ref_obj.h"
#include "slab_cache.h"
#include <cxx/slist>

class Ram_quota;
class Context;
class Kobject;

class Space;

typedef Generic_obj_space<Space> Obj_space;

class Space
: public Ref_cnt_obj,
  public Mem_space,
  public Generic_obj_space<Space>
{
  MEMBER_OFFSET();
  friend class Jdb_space;

public:

  struct Caps
  : cxx::int_type_base<unsigned char, Caps>,
    cxx::int_bit_ops<Caps>,
    cxx::int_null_chk<Caps>
  {
    Caps() = default;
    explicit constexpr Caps(unsigned char v)
    : cxx::int_type_base<unsigned char, Caps>(v) {}

    static constexpr Caps none() { return Caps(0); }
    static constexpr Caps mem() { return Caps(1); }
    static constexpr Caps obj() { return Caps(2); }
    static constexpr Caps io() { return Caps(4); }
    static constexpr Caps threads() { return Caps(8); }
    static constexpr Caps all() { return Caps(0xf); }
  };

  explicit Space(Ram_quota *q, Caps c) : Mem_space(q), _caps(c) {}
  virtual ~Space() = 0;

  enum State
  { // we must use values with the two lest significant bits == 0
    Starting    = 0x00,
    Ready       = 0x08,
    In_deletion = 0x10
  };

  struct Ku_mem : public cxx::S_list_item
  {
    User<void>::Ptr u_addr;
    void *k_addr;
    unsigned size;

    static Slab_cache *a;

    void *operator new (size_t, Ram_quota *q) throw()
    { return a->q_alloc(q); }

    void free(Ram_quota *q) throw()
    { a->q_free(q, this); }

    template<typename T>
    T *kern_addr(Smart_ptr<T, Simple_ptr_policy, User_ptr_discr> ua) const
    {
      typedef Address A;
      return (T*)((A)ua.get() - (A)u_addr.get() + (A)k_addr);
    }
  };

  Caps caps() const { return _caps; }

  void switchin_context(Space *from);

protected:
  Space(Ram_quota *q, Mem_space::Dir_type* pdir, Caps c)
  : Mem_space(q, pdir), _caps(c) {}

  const Caps _caps;

protected:
  typedef cxx::S_list<Ku_mem> Ku_mem_list;
  Ku_mem_list _ku_mem;
};


//---------------------------------------------------------------------------
IMPLEMENTATION:

#include "assert.h"
#include "atomic.h"
#include "lock_guard.h"
#include "config.h"
#include "globalconfig.h"
#include "l4_types.h"

//
// class Space
//

PUBLIC inline Ram_quota * Space::ram_quota() const
{ return Mem_space::ram_quota(); }

IMPLEMENT inline Space::~Space() {}

PUBLIC
Space::Ku_mem const *
Space::find_ku_mem(User<void>::Ptr p, unsigned size)
{
  Address const pa = (Address)p.get();

  // alignment check
  if (EXPECT_FALSE(pa & (sizeof(double) - 1)))
    return 0;

  // overflow check
  if (EXPECT_FALSE(pa + size < pa))
    return 0;

  for (Ku_mem_list::Const_iterator f = _ku_mem.begin(); f != _ku_mem.end(); ++f)
    {
      Address const a = (Address)f->u_addr.get();
      if (a <= pa && (a + f->size) >= (pa + size))
	return *f;
    }

  return 0;
}

IMPLEMENT_DEFAULT inline
void
Space::switchin_context(Space *from)
{
  Mem_space::switchin_context(from);
}


PUBLIC static inline
bool
Space::is_user_memory(Address address, Mword len)
{
  return    address <= Mem_layout::User_max && len > 0
         && Mem_layout::User_max - address >= len - 1;
}

