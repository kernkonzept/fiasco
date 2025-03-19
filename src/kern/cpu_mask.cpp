INTERFACE:

#include "bitmap.h"
#include "config.h"
#include "types.h"

template<unsigned MAX_NUM_CPUS>
class Cpu_mask_t
{
public:
  enum { Max_num_cpus = MAX_NUM_CPUS };
  enum class Init { Bss };

  constexpr Cpu_mask_t(Init) {}
  constexpr Cpu_mask_t() { _b.clear_all(); }

  Cpu_mask_t(Cpu_mask_t const &) = default;
  Cpu_mask_t &operator = (Cpu_mask_t const &) = default;

  template<unsigned NCPUS>
  Cpu_mask_t &operator = (Cpu_mask_t<NCPUS> const &o)
  {
    _b = o._b;
    return *this;
  }

  bool empty() const { return _b.is_empty(); }

  bool get(Cpu_number cpu) const
  { return _b[cxx::int_value<Cpu_number>(cpu)]; }

  void clear(Cpu_number cpu)
  { _b.clear_bit(cxx::int_value<Cpu_number>(cpu)); }

  void set(Cpu_number cpu)
  { _b.set_bit(cxx::int_value<Cpu_number>(cpu)); };

  Cpu_mask_t &operator |= (Cpu_mask_t const &o)
  {
    _b |= o._b;
    return *this;
  }

  Cpu_mask_t &operator &= (Cpu_mask_t const &o)
  {
    _b &= o._b;
    return *this;
  }

  void atomic_set(Cpu_number cpu)
  { _b.atomic_set_bit(cxx::int_value<Cpu_number>(cpu)); }

  void atomic_set_if_unset(Cpu_number cpu)
  { _b.atomic_get_and_set_if_unset(cxx::int_value<Cpu_number>(cpu)); }

  bool atomic_get_and_set_if_unset(Cpu_number cpu)
  { return _b.atomic_get_and_set_if_unset(cxx::int_value<Cpu_number>(cpu)); }

  void atomic_clear(Cpu_number cpu)
  { _b.atomic_clear_bit(cxx::int_value<Cpu_number>(cpu)); }

  void atomic_clear_if_set(Cpu_number cpu)
  { _b.atomic_get_and_clear_if_set(cxx::int_value<Cpu_number>(cpu)); }

  bool atomic_get_and_clear(Cpu_number cpu)
  { return _b.atomic_get_and_clear(cxx::int_value<Cpu_number>(cpu)); }

  void atomic_or(Cpu_mask_t const &v)
  { _b.atomic_or(v._b); }

private:
  template<unsigned B> friend class Cpu_mask_t;
  Bitmap<Max_num_cpus> _b;
};

typedef Cpu_mask_t<Config::Max_num_cpus> Cpu_mask;
