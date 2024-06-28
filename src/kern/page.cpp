INTERFACE:

#include "types.h"
#include "l4_msg_item.h"

class Page
{
public:

  /* These things must be defined in arch part in
     the most efficient way according to the architecture.

  enum Attribs_enum : Mword {
    USER_NO  = xxx, ///< User No access
    USER_RO  = xxx, ///< User Read only
    USER_RW  = xxx, ///< User Read/Write
    USER_RX  = xxx, ///< User Read/Execute
    USER_XO  = xxx, ///< User Execute only
    USER_RWX = xxx, ///< User Read/Write/Execute

    NONCACHEABLE = xxx, ///< Caching is off
    CACHEABLE    = xxx, ///< Caching is enabled
  };

  */

  typedef L4_fpage::Rights Rights;
  typedef L4_msg_item::Memory_type Type;

  struct Kern
  : cxx::int_type_base<unsigned char, Kern>,
    cxx::int_bit_ops<Kern>,
    cxx::int_null_chk<Kern>
  {
    Kern() = default;
    explicit constexpr Kern(Value v) : cxx::int_type_base<unsigned char, Kern>(v) {}

    static constexpr Kern None() { return Kern(0); }
    static constexpr Kern Global() { return Kern(1); }
  };

  struct Flags
  : cxx::int_type_base<unsigned char, Flags>,
    cxx::int_bit_ops<Flags>,
    cxx::int_null_chk<Flags>
  {
    Flags() = default;
    explicit constexpr Flags(Value v) : cxx::int_type_base<unsigned char, Flags>(v) {}

    static constexpr Flags None() { return Flags(0); }
    static constexpr Flags Referenced() { return Flags(1); }
    static constexpr Flags Dirty() { return Flags(2); }
    static constexpr Flags Touched() { return Referenced() | Dirty(); }
  };

  struct Attr
  {
    Rights rights;
    Type type;
    Kern kern;
    Flags flags;

    Attr() = default;
    explicit constexpr Attr(Rights r, Type t, Kern k, Flags f)
    : rights(r), type(t), kern(k), flags(f) {}

    // per-space local mapping, "normally" cached
    static constexpr Attr space_local(Rights r)
    { return Attr(r, Type::Normal(), Kern::None(), Flags::None()); }

    // global kernel mapping, "normally" cached
    static constexpr Attr kern_global(Rights r)
    { return Attr(r, Type::Normal(), Kern::Global(), Flags::None()); }

    static constexpr Attr writable()
    { return Attr(Rights::W(), Type::Normal(), Kern::None(), Flags::None()); }

    Attr apply(Attr o) const
    {
      Attr n = *this;
      n.rights &= o.rights;

      if ((o.type & Type::Set()) == Type::Set())
        n.type = o.type & ~Type::Set();

      return n;
    }

    constexpr bool empty() const
    { return (rights & Rights::RWX()).empty(); }

    Attr operator |= (Attr r)
    {
      rights |= r.rights;
      return *this;
    }
  };
};
