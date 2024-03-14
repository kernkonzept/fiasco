/*
 * ARM Kernel-Info Page
 */

INTERFACE [arm]:

#include "config.h"
#include "cxx/static_vector"
#include "types.h"

EXTENSION class Kip
{
public:
  class Kip_array
  {
    Kip *_first;
    unsigned _length;
    enum : unsigned { Kip_size = Config::PAGE_SIZE };

  public:
    class Iterator
    {
      friend class Kip_array;
      Iterator(Kip *i) : _i(i) {}
      Kip *_i;

    public:
      bool operator==(Iterator const &other) const { return _i == other._i; }
      bool operator!=(Iterator const &other) const { return _i != other._i; }
      Kip *operator*() const { return _i; }
      Kip *operator->() const { return _i; }

      Iterator &operator++()
      {
        _i = offset_cast<Kip *>(_i, Kip_size);
        return *this;
      }
    };

    Kip_array(Kip *first, unsigned length)
    : _first(first), _length(length)
    {}

    Iterator begin() const { return Iterator(_first); }
    Iterator end() const { return Iterator(offset_cast<Kip *>(_first, Kip_size * _length)); }

    Kip *operator[](unsigned i)
    { return offset_cast<Kip *>(_first, Kip_size * i); }
  };

  struct Platform_info
  {
    char name[16];
    Unsigned32 is_mp;
    Unsigned32 arch_cpuinfo[23];
  };

  /* 0xF0 / 0x1E0 */
  Platform_info platform_info;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "amp_node.h"

PUBLIC static inline NEEDS["amp_node.h"]
Kip::Kip_array
Kip::all_instances()
{
  extern char my_kernel_info_page[];
  Kip *first = reinterpret_cast<Kip*>(my_kernel_info_page);
  return Kip_array(first, Amp_node::Max_num_nodes);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && debug]:

IMPLEMENT inline
void
Kip::debug_print_syscalls() const
{}
