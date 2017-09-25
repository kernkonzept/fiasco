INTERFACE:

#include <acpi.h>

namespace ACPI {

struct Dmar_head
{
  Dmar_head const *next() const
  { return reinterpret_cast<Dmar_head const *>((char const *)this + length); }

  template<typename T>
  T const *cast() const
  {
    if (type == T::Id)
      return static_cast<T const *>(this);
    return 0;
  }

  Unsigned16 type;
  Unsigned16 length;
} __attribute__((packed));

template<typename T>
struct Next_iter
{
  Next_iter() : _c(0) {}
  Next_iter(T const *h) : _c(h) {}
  Next_iter const &operator ++ ()
  {
    _c = _c->next();
    return *this;
  }
  T const &operator * () const { return *_c; }

  bool operator == (Next_iter const &o) const { return _c == o._c; }
  bool operator != (Next_iter const &o) const { return _c != o._c; }

private:
  T const *_c;
};

struct Dmar_dev_scope
{
  Dmar_dev_scope const *next() const
  {
    return reinterpret_cast<Dmar_dev_scope const *>((char const *)this + length);
  }

  struct Path_entry
  {
    Unsigned8 dev;
    Unsigned8 func;
  };

  Path_entry const *begin() const { return path; }
  Path_entry const *end() const
  { return reinterpret_cast<Path_entry const *>((char const *)this + length); }

  Unsigned8 type;
  Unsigned8 length;
  Unsigned16 _rsvd;
  Unsigned8 enum_id;
  Unsigned8 start_bus_nr;
  Path_entry path[];
} __attribute__((packed));

struct Dev_scope_vect
{
  typedef Next_iter<Dmar_dev_scope> Iterator;

  Dev_scope_vect() = default;
  Dev_scope_vect(Iterator b, Iterator e)
  : _begin(b), _end(e)
  {}

  Iterator begin() { return _begin; }
  Iterator end() { return _end; }

private:
  Iterator _begin, _end;
};

template<typename TYPE>
struct Dmar_dev_scope_mixin
{
  Dev_scope_vect devs() const
  {
    TYPE const *t = static_cast<TYPE const*>(this);
    return Dev_scope_vect(reinterpret_cast<Dmar_dev_scope const *>(t + 1),
                          reinterpret_cast<Dmar_dev_scope const *>((char const *)t + t->length));
  }
};

struct Dmar_drhd : Dmar_head, Dmar_dev_scope_mixin<Dmar_drhd>
{
  enum { Id = 0 };
  Unsigned8  flags;
  Unsigned8  _rsvd;
  Unsigned16 segment;
  Unsigned64 register_base;
} __attribute__((packed));

struct Dmar_rmrr : Dmar_head, Dmar_dev_scope_mixin<Dmar_rmrr>
{
  enum { Id = 1 };
  Unsigned16 _rsvd;
  Unsigned16 segment;
  Unsigned64 base;
  Unsigned64 limit;
} __attribute__((packed));

struct Dmar_atsr : Dmar_head, Dmar_dev_scope_mixin<Dmar_atsr>
{
  enum { Id = 2 };
  Unsigned8  flags;
  Unsigned8  _rsvd;
  Unsigned16 segment;
} __attribute__((packed));

struct Dmar_rhsa : Dmar_head
{
  enum { Id = 3 };

private:
  Unsigned32 _rsvd;
  Unsigned64 register_base;
  Unsigned32 proximity_domain;
} __attribute__((packed));

struct Dmar_andd : Dmar_head
{
  enum { Id = 4 };

private:
  Unsigned8 _rsvd[3];
  Unsigned8 acpi_dev_nr;
  Unsigned8 acpi_name[];
} __attribute__((packed));

} // namespace ACPI

struct Acpi_dmar : Acpi_table_head
{
  typedef ACPI::Next_iter<ACPI::Dmar_head> Iterator;

  Iterator begin() const
  { return reinterpret_cast<ACPI::Dmar_head const *>(this + 1); }

  Iterator end() const
  { return reinterpret_cast<ACPI::Dmar_head const *>((char const *)this + len); }

  struct Flags
  {
    Unsigned8 raw;
    CXX_BITFIELD_MEMBER( 0, 0, intr_remap, raw);
    CXX_BITFIELD_MEMBER( 1, 1, x2apic_opt_out, raw);
  };

  Unsigned8 haw; ///< Host Address Width
  Flags flags;
  Unsigned8 _rsvd[10];
} __attribute__((packed));


