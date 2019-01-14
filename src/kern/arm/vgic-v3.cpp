INTERFACE [vgic && cpu_virt]:

#include "types.h"

#include <cxx/bitfield>

EXTENSION class Gic_h
{
public:
  enum { Version = 3 };

  struct Lr
  {
    Unsigned64 raw;
    Lr() = default;
    explicit Lr(Unsigned32 v) : raw(v) {}
  };

  struct Aprs
  {
    enum { N_aprs = 4 }; // 32 prios is max

    Unsigned32 ap0r[N_aprs];
    Unsigned32 ap1r[N_aprs];
    void clear()
    {
      for (auto &a: ap0r)
        a = 0;

      for (auto &a: ap1r)
        a = 0;
    }
  };

  // fake pointer to call static functions
  static Gic_h const *const gic;
  static unsigned n_aprs;
};

IMPLEMENTATION:

#include "static_init.h"

unsigned Gic_h::n_aprs = 1U << (Gic_h::vtr().pri_bits() - 4);

