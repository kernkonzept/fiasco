INTERFACE:

template< typename T, unsigned EXTRA = 0 >
class Per_cpu_array
: public cxx::array<T, Cpu_number, Config::Max_num_cpus + EXTRA>
{};

INTERFACE [mp]:

#include "types.h"
#include <cassert>

struct Per_cpu_ctor_data
{
  typedef void (*Func)(void *, Cpu_number);

  void exec(Cpu_number cpu) const
  {
    _func(_base, cpu);
  }

  Per_cpu_ctor_data() = default;
  Per_cpu_ctor_data(Func ctor, void *base)
  : _func(ctor), _base(base)
  {}


private:
  Func _func;
  void *_base;
};

#define DEFINE_PER_CPU_CTOR_UID(b) __per_cpu_ctor_ ## b
#define DEFINE_PER_CPU_CTOR_DATA(id) \
  __attribute__((section(".bss.per_cpu_ctor_data"),used)) \
    static Per_cpu_ctor_data DEFINE_PER_CPU_CTOR_UID(id);

INTERFACE [!mp]:

#define DEFINE_PER_CPU_CTOR_DATA(id)

INTERFACE:

#include "static_init.h"
#include "config.h"
#include "context_base.h"
#include <cxx/type_traits>

#define DEFINE_PER_CPU_P(p) \
  DEFINE_PER_CPU_CTOR_DATA(__COUNTER__) \
  __attribute__((section(".per_cpu.data"),init_priority(0xfffe - p)))

#define DEFINE_PER_CPU      DEFINE_PER_CPU_P(9)
#define DEFINE_PER_CPU_LATE DEFINE_PER_CPU_P(19)

class Per_cpu_data
{
public:
  static void init_ctors();
  static void run_ctors(Cpu_number cpu);
  static void run_late_ctors(Cpu_number cpu);
  static bool valid(Cpu_number cpu);

  enum With_cpu_num { Cpu_num };
};

template< typename T > class Per_cpu_ptr;

template< typename T >
class Per_cpu : private Per_cpu_data
{
  friend class Per_cpu_ptr<T>;
public:
  typedef T Type;

  T const &cpu(Cpu_number) const;
  T &cpu(Cpu_number);

  T const &current() const { return cpu(current_cpu()); }
  T &current() { return cpu(current_cpu()); }

  Per_cpu();
  explicit Per_cpu(With_cpu_num);

  template<typename TEST>
  Cpu_number find_cpu(TEST const &test) const
  {
    for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
      if (valid(i) && test(cpu(i)))
        return i;

    return Cpu_number::nil();
  }

private:
  T _d;
};

template< typename T >
class Per_cpu_ptr : private Per_cpu_data
{
public:
  typedef typename cxx::conditional<
    cxx::is_const<T>::value,
    Per_cpu<typename cxx::remove_cv<T>::type> const,
    Per_cpu<typename cxx::remove_cv<T>::type> >::type Per_cpu_type;

  Per_cpu_ptr() {}
  Per_cpu_ptr(Per_cpu_type *o) : _p(&o->_d) {}
  Per_cpu_ptr &operator = (Per_cpu_type *o)
  {
    _p = &o->_d;
    return *this;
  }

  T &cpu(Cpu_number cpu);
  T &current() { return cpu(current_cpu()); }

private:
  T *_p;
};


//---------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

#include <construction.h>

IMPLEMENT inline
bool
Per_cpu_data::valid(Cpu_number cpu)
{
#if defined NDEBUG
  (void)cpu;
  return 1;
#else
  return cpu == Cpu_number::boot_cpu();
#endif
}

IMPLEMENT inline
template< typename T >
T const &Per_cpu<T>::cpu(Cpu_number) const { return _d; }

IMPLEMENT inline
template< typename T >
T &Per_cpu<T>::cpu(Cpu_number) { return _d; }

IMPLEMENT
template< typename T >
Per_cpu<T>::Per_cpu()
{}

IMPLEMENT
template< typename T >
Per_cpu<T>::Per_cpu(With_cpu_num) : _d(Cpu_number::boot_cpu())
{}

IMPLEMENT inline
template< typename T >
T &Per_cpu_ptr<T>::cpu(Cpu_number) { return *_p; }


IMPLEMENT
void
Per_cpu_data::init_ctors()
{
}

IMPLEMENT inline
void
Per_cpu_data::run_ctors(Cpu_number)
{
  extern ctor_function_t __PER_CPU_INIT_ARRAY_START__[];
  extern ctor_function_t __PER_CPU_INIT_ARRAY_END__[];
  run_ctor_functions(__PER_CPU_INIT_ARRAY_START__, __PER_CPU_INIT_ARRAY_END__);

  extern ctor_function_t __PER_CPU_CTORS_LIST__[];
  extern ctor_function_t __PER_CPU_CTORS_END__[];
  run_ctor_functions(__PER_CPU_CTORS_LIST__, __PER_CPU_CTORS_END__);
}

IMPLEMENT inline
void
Per_cpu_data::run_late_ctors(Cpu_number)
{
  extern ctor_function_t __PER_CPU_LATE_INIT_ARRAY_START__[];
  extern ctor_function_t __PER_CPU_LATE_INIT_ARRAY_END__[];
  run_ctor_functions(__PER_CPU_LATE_INIT_ARRAY_START__,
                     __PER_CPU_LATE_INIT_ARRAY_END__);

  extern ctor_function_t __PER_CPU_LATE_CTORS_LIST__[];
  extern ctor_function_t __PER_CPU_LATE_CTORS_END__[];
  run_ctor_functions(__PER_CPU_LATE_CTORS_LIST__, __PER_CPU_LATE_CTORS_END__);
}


//---------------------------------------------------------------------------
INTERFACE [mp]:

#include <cstddef>
#include <new>

#include "config.h"

EXTENSION
class Per_cpu_data
{
private:
  typedef Per_cpu_ctor_data Ctor;

  struct Ctor_vector
  {
    void push_back(Ctor::Func func, void *base);
    unsigned len() const { return _len; }
    Ctor const &operator [] (unsigned idx) const
    {
      extern Ctor _per_cpu_ctor_data_start[];
      return _per_cpu_ctor_data_start[idx];
    }

  private:
    unsigned _len;
  };

protected:
  enum { Num_cpus = Config::Max_num_cpus + 1 }; // add one for the never running CPU
  static Cpu_number num_cpus() { return Cpu_number(Num_cpus); }
  typedef Per_cpu_array<long, 1> Offset_array;
  static Offset_array _offsets;
  static unsigned late_ctor_start;
  static Ctor_vector ctors;
};


//---------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "panic.h"
#include <construction.h>
#include <cstring>

Per_cpu_data::Offset_array Per_cpu_data::_offsets;
unsigned Per_cpu_data::late_ctor_start;
Per_cpu_data::Ctor_vector Per_cpu_data::ctors;

IMPLEMENT
void
Per_cpu_data::Ctor_vector::push_back(Ctor::Func func, void *base)
{
  extern Ctor _per_cpu_ctor_data_start[];
  extern Ctor _per_cpu_ctor_data_end[];

  if (_per_cpu_ctor_data_start + _len >= _per_cpu_ctor_data_end)
    panic("out of per_cpu_ctor_space");

  _per_cpu_ctor_data_start[_len++] = Ctor(func, base);
}


IMPLEMENT inline
bool
Per_cpu_data::valid(Cpu_number cpu)
{ return cpu < num_cpus() && _offsets[cpu] != -1; }

IMPLEMENT inline template< typename T >
T const &Per_cpu<T>::cpu(Cpu_number cpu) const
{ return *reinterpret_cast<T const *>((char  const *)&_d + _offsets[cpu]); }

IMPLEMENT inline template< typename T >
T &Per_cpu<T>::cpu(Cpu_number cpu)
{ return *reinterpret_cast<T*>((char *)&_d + _offsets[cpu]); }

IMPLEMENT
template< typename T >
Per_cpu<T>::Per_cpu()
{
  //printf("  Per_cpu<T>() [this=%p])\n", this);
  ctors.push_back(&ctor_wo_arg, this);
}

IMPLEMENT
template< typename T >
Per_cpu<T>::Per_cpu(With_cpu_num) : _d(Cpu_number::boot_cpu())
{
  //printf("  Per_cpu<T>(bool) [this=%p])\n", this);
  ctors.push_back(&ctor_w_arg, this);
}

PRIVATE static
template< typename T >
void Per_cpu<T>::ctor_wo_arg(void *obj, Cpu_number cpu)
{
  //printf("Per_cpu<T>::ctor_wo_arg(obj=%p, cpu=%u -> %p)\n", obj, cpu, &(reinterpret_cast<Per_cpu<T>*>(obj)->cpu(cpu)));
  new (&reinterpret_cast<Per_cpu<T>*>(obj)->cpu(cpu)) T;
}

PRIVATE static
template< typename T >
void Per_cpu<T>::ctor_w_arg(void *obj, Cpu_number cpu)
{
  //printf("Per_cpu<T>::ctor_w_arg(obj=%p, cpu=%u -> %p)\n", obj, cpu, &reinterpret_cast<Per_cpu<T>*>(obj)->cpu(cpu));
  new (&reinterpret_cast<Per_cpu<T>*>(obj)->cpu(cpu)) T(cpu);
}

IMPLEMENT inline
template< typename T >
T &Per_cpu_ptr<T>::cpu(Cpu_number cpu)
{ return *reinterpret_cast<T *>(reinterpret_cast<Address>(_p) + _offsets[cpu]); }

IMPLEMENT
void
Per_cpu_data::init_ctors()
{
  for (Offset_array::iterator i = _offsets.begin(); i != _offsets.end(); ++i)
    *i = -1;
}

IMPLEMENT inline
void
Per_cpu_data::run_ctors(Cpu_number cpu)
{
  extern ctor_function_t __PER_CPU_INIT_ARRAY_START__[];
  extern ctor_function_t __PER_CPU_INIT_ARRAY_END__[];
  extern ctor_function_t __PER_CPU_CTORS_LIST__[];
  extern ctor_function_t __PER_CPU_CTORS_END__[];
  if (cpu == Cpu_number::boot_cpu())
    {
      run_ctor_functions(__PER_CPU_INIT_ARRAY_START__, __PER_CPU_INIT_ARRAY_END__);
      run_ctor_functions(__PER_CPU_CTORS_LIST__, __PER_CPU_CTORS_END__);
      late_ctor_start = ctors.len();
      return;
    }

  for (unsigned i = 0; i < late_ctor_start; ++i)
    ctors[i].exec(cpu);
}

IMPLEMENT inline
void
Per_cpu_data::run_late_ctors(Cpu_number cpu)
{
  extern ctor_function_t __PER_CPU_LATE_INIT_ARRAY_START__[];
  extern ctor_function_t __PER_CPU_LATE_INIT_ARRAY_END__[];
  extern ctor_function_t __PER_CPU_LATE_CTORS_LIST__[];
  extern ctor_function_t __PER_CPU_LATE_CTORS_END__[];
  if (cpu == Cpu_number::boot_cpu())
    {
      run_ctor_functions(__PER_CPU_LATE_INIT_ARRAY_START__, __PER_CPU_LATE_INIT_ARRAY_END__);
      run_ctor_functions(__PER_CPU_LATE_CTORS_LIST__, __PER_CPU_LATE_CTORS_END__);
      return;
    }

  unsigned c = ctors.len();
  for (unsigned i = late_ctor_start; i < c; ++i)
    ctors[i].exec(cpu);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [mp && debug]:

PUBLIC static
long
Per_cpu_data::offset(Cpu_number cpu)
{
  return _offsets[cpu];
}
