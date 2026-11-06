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
  Cpu_mask_t &operator = (Cpu_mask_t<NCPUS> const &o) &
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

  Cpu_mask_t &operator |= (Cpu_mask_t const &o) &
  {
    _b |= o._b;
    return *this;
  }

  Cpu_mask_t &operator &= (Cpu_mask_t const &o) &
  {
    _b &= o._b;
    return *this;
  }

  friend bool operator == (Cpu_mask_t const &, Cpu_mask_t const &) = default;

  template<unsigned NCPUS>
  friend bool operator == (Cpu_mask_t const &lhs, Cpu_mask_t<NCPUS> const &rhs)
  { return lhs._b == rhs._b; }

  friend bool operator <= (Cpu_mask_t const &lhs, Cpu_mask_t const &rhs)
  { return lhs._b <= rhs._b; }

  template<unsigned NCPUS>
  friend bool operator <= (Cpu_mask_t const &lhs, Cpu_mask_t<NCPUS> const &rhs)
  { return lhs._b <= rhs._b; }

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

  /**
   * Iterator over the CPUs set in the mask.
   *
   * Bits changed concurrently are observed when the iterator reaches them.
   */
  class Iterator
  {
  public:
    Iterator(Cpu_mask_t const *m, unsigned cpu) : _m(m), _cpu(m->next(cpu)) {}
    Cpu_number operator * () const { return Cpu_number(_cpu); }
    Iterator &operator ++ () { _cpu = _m->next(_cpu + 1); return *this; }
    friend bool operator == (Iterator const &l, Iterator const &r)
    { return l._cpu == r._cpu; }

  private:
    Cpu_mask_t const *_m;
    unsigned _cpu;
  };

  Iterator begin() const { return Iterator(this, 0); }
  Iterator end() const { return Iterator(this, Nr_iter_cpus); }

private:
  // We must not iterate over the invalid CPU number that is part of the
  // Cpu::Online_cpu_mask; therefore we must not use MAX_NUM_CPUS.
  static constexpr unsigned Nr_iter_cpus =
    MAX_NUM_CPUS < Config::Max_num_cpus ? MAX_NUM_CPUS : Config::Max_num_cpus;

  /// Get the first CPU set in the mask starting at `cpu`, or Nr_iter_cpus if
  /// none.
  unsigned next(unsigned cpu) const
  {
    if (cpu >= Nr_iter_cpus)
      return Nr_iter_cpus;

    size_t r = _b.ffs(cpu);
    return (r == 0 || r > Nr_iter_cpus) ? Nr_iter_cpus : r - 1;
  }

  template<unsigned B> friend class Cpu_mask_t;
  Bitmap<Max_num_cpus> _b;
};

typedef Cpu_mask_t<Config::Max_num_cpus> Cpu_mask;
