INTERFACE [!io]:

class Io_space
{
};

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
    // alias threads until there are other KUmem usecases
    static constexpr Caps kumem() { return Caps(8); }
    static constexpr Caps all() { return Caps(0xf); }
  };

  explicit Space(Ram_quota *q, Caps c) : Mem_space(q), _caps(c) {}
  virtual ~Space() = 0;

  struct Ku_mem : public cxx::S_list_item
  {
    User_ptr<void> u_addr;
    void *k_addr;
    unsigned size;

    void *operator new (size_t, Ram_quota *q) noexcept
    { return Space::alloc_ku_mem(q); }

    void free(Ram_quota *q) noexcept
    { Space::free_ku_mem(q, this); }

    template<typename T>
    T *kern_addr(User_ptr<T> ua) const
    {
      return reinterpret_cast<T*>(reinterpret_cast<Address>(ua.get())
                                  - reinterpret_cast<Address>(u_addr.get())
                                  + reinterpret_cast<Address>(k_addr));
    }
  };

  Caps caps() const { return _caps; }

  void switchin_context(Space *from, Mem_space::Switchin_flags flags = None);

protected:
  Space(Ram_quota *q, Mem_space::Dir_type* pdir, Caps c)
  : Mem_space(q, pdir), _caps(c) {}

  bool initialize()
  {
    return Mem_space::initialize() && Generic_obj_space<Space>::initialize();
  }

  const Caps _caps;

protected:
  typedef cxx::S_list<Ku_mem> Ku_mem_list;
  Ku_mem_list _ku_mem;
};


//---------------------------------------------------------------------------
IMPLEMENTATION:

#include "atomic.h"
#include "lock_guard.h"
#include "config.h"
#include "globalconfig.h"
#include "l4_types.h"
#include "kmem_slab.h"
#include "global_data.h"

static DEFINE_GLOBAL
Global_data<Kmem_slab_t<Space::Ku_mem>> _k_u_mem_list_alloc("Ku_mem");

//
// class Space
//

PUBLIC inline Ram_quota * Space::ram_quota() const
{ return Mem_space::ram_quota(); }

IMPLEMENT inline Space::~Space() {}

PUBLIC
Space::Ku_mem const *
Space::find_ku_mem(User_ptr<void> p, unsigned size)
{
  Address const pa = reinterpret_cast<Address>(p.get());

  // alignment check
  if (EXPECT_FALSE(pa & (sizeof(double) - 1)))
    return nullptr;

  // overflow check
  if (EXPECT_FALSE(pa + size - 1 < pa))
    return nullptr;

  for (Ku_mem_list::Const_iterator f = _ku_mem.begin(); f != _ku_mem.end(); ++f)
    {
      Address const a = reinterpret_cast<Address>(f->u_addr.get());
      if (a <= pa && (a + f->size - 1) >= (pa + size - 1))
	return *f;
    }

  return nullptr;
}

IMPLEMENT_DEFAULT inline
void
Space::switchin_context(Space *from, Mem_space::Switchin_flags flags)
{
  Mem_space::switchin_context(from, flags);
}


PUBLIC static inline
bool
Space::is_user_memory(Address address, Mword len)
{
  return    address <= Mem_layout::User_max && len > 0
         && Mem_layout::User_max - address >= len - 1;
}

PRIVATE static
void *
Space::alloc_ku_mem(Ram_quota *q) noexcept
{ return _k_u_mem_list_alloc->q_alloc(q); }

PRIVATE static
void
Space::free_ku_mem(Ram_quota *q, void *k) noexcept
{ _k_u_mem_list_alloc->q_free(q, k); }

//--------------------------------------------------------------------------
IMPLEMENTATION [!io]:

/**
 * Empty IO space context switchin implementation.
 *
 * This empty method is here to streamline code that might or might not support
 * IO spaces depending on the compile-time configuration.
 */
PUBLIC
inline
void
Io_space::switchin_context(Space *)
{
}
