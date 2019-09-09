INTERFACE[debug]:

#include "types.h"

class Space;

class Jdb_address
{
protected:
  Space *_space;
  Address _addr;

public:
  Jdb_address() = default;
  Jdb_address(Address virt, Space *space) : _space(space), _addr(virt) {}
  Jdb_address(void const *virt, Space *space) : _space(space), _addr((Address)virt) {}
  explicit Jdb_address(Address phys) : _space((Space *)1), _addr(phys) {}
  static Jdb_address kmem_addr(Address kaddr)
  { return Jdb_address(kaddr, nullptr); }

  static Jdb_address kmem_addr(void *kaddr)
  { return Jdb_address(kaddr, nullptr); }

  static Jdb_address null() { return Jdb_address(nullptr, nullptr); }

  bool is_phys() const { return _space == (Space *)1; }
  bool is_kmem() const { return _space == nullptr; }
  bool is_null() const { return _space == nullptr && _addr == 0; }
  Address phys() const { return _addr; }
  Address addr() const { return _addr; }
  void *virt() const { return (void *)_addr; }

  Space *space() const { return _space; }

  Jdb_address operator -= (long diff)
  {
    _addr -= diff;
    return *this;
  }

  Jdb_address operator += (long diff)
  {
    _addr += diff;
    return *this;
  }

  Jdb_address operator ++ ()
  {
    ++_addr;
    return *this;
  }

  Jdb_address operator ++ (int)
  {
    Jdb_address r = *this;
    ++_addr;
    return r;
  }

  bool operator >= (Jdb_address const &o) const
  { return addr() >= o.addr(); }

  bool operator > (Jdb_address const &o) const
  { return addr() > o.addr(); }

  bool operator <= (Jdb_address const &o) const
  { return addr() <= o.addr(); }

  bool operator < (Jdb_address const &o) const
  { return addr() < o.addr(); }

  bool operator == (Jdb_address const &o) const
  { return (space() == o.space()) && (addr() == o.addr()); }
};


template<typename T>
class Jdb_addr : public Jdb_address
{
public:
  Jdb_addr() = default;
  Jdb_addr(T *virt, Space *space) : Jdb_address(virt, space) {}
  explicit Jdb_addr(Address phys) : Jdb_address(phys) {}
  static Jdb_addr<T> kmem_addr(T *kaddr)
  { return Jdb_addr<T>(kaddr, nullptr); }

  static Jdb_addr<T> null()
  { return Jdb_addr<T>(nullptr, nullptr); }


  T *virt() const { return (T *)Jdb_address::virt(); }

  Jdb_addr operator -= (long diff)
  {
    _addr -= diff * sizeof(T);
    return *this;
  }

  Jdb_addr operator += (long diff)
  {
    _addr += diff * sizeof(T);
    return *this;
  }

  Jdb_addr operator ++ ()
  {
    _addr += sizeof(T);
    return *this;
  }

  Jdb_addr operator ++ (int)
  {
    Jdb_addr<T> r = *this;
    _addr += sizeof(T);
    return r;
  }
};


IMPLEMENTATION[debug]:

inline
Jdb_address operator + (Jdb_address const &a, long diff)
{
  return Jdb_address(a.addr() + diff, a.space());
}

inline
Jdb_address operator - (Jdb_address const &a, long diff)
{
  return Jdb_address(a.addr() - diff, a.space());
}

template<typename T> inline
Jdb_addr<T> operator + (Jdb_addr<T> const &a, long diff)
{
  return Jdb_addr<T>(a.virt() + diff, a.space());
}

template<typename T> inline
Jdb_addr<T> operator - (Jdb_addr<T> const &a, long diff)
{
  return Jdb_addr<T>(a.virt() - diff, a.space());
}
