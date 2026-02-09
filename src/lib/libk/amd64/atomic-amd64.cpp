INTERFACE [amd64]:

// preprocess off
#define ATOMIC_OP_(op, size, suffix)                                           \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  void                                                                         \
  atomic_##op(T *mem, V value)                                                 \
  {                                                                            \
    T val = value;                                                             \
    asm volatile (                                                             \
      "lock; "#op #suffix" %1, %2"                                             \
      : "=m"(*mem)                                                             \
      : "er"(val), "m"(*mem));                                                 \
  }

#define ATOMIC_OP(op)  \
  ATOMIC_OP_(op, 4, l) \
  ATOMIC_OP_(op, 8, q)

ATOMIC_OP(or)
ATOMIC_OP(and)
ATOMIC_OP(add)
#undef ATOMIC_OP_
#undef ATOMIC_OP
// preprocess on

//----------------------------------------------------------------------------
IMPLEMENTATION [amd64]:

template<typename T, typename V> inline
T
atomic_fetch_and(T *mem, V value)
{
  T val = value;
  T old;
  do
    {
      old = *mem;
    }
  while (!cas(mem, old, old & val));
  return old;
}

template<typename T, typename V> inline
T
atomic_and_fetch(T *mem, V value)
{
  T val = value;
  T old;
  do
    {
      old = *mem;
    }
  while (!cas(mem, old, old & val));
  return old & val;
}

template<typename T, typename V> inline
T
atomic_fetch_or(T *mem, V value)
{
  T val = value;
  T old;
  do
    {
      old = *mem;
    }
  while (!cas(mem, old, old | val));
  return old;
}

template<typename T, typename V> inline
T
atomic_or_fetch(T *mem, V value)
{
  T val = value;
  T old;
  do
    {
      old = *mem;
    }
  while (!cas(mem, old, old | val));
  return old | val;
}

template<typename T, typename V>
requires(sizeof(T) == 4 || sizeof(T) == 8) inline
T
atomic_fetch_add(T *mem, V value)
{
  T val = value;
  T old;
  asm volatile ("lock; xadd %1, %0" : "+m"(*mem), "=r"(old) : "1"(val));
  return old;
}

template<typename T, typename V>
requires(sizeof(T) == 4 || sizeof(T) == 8) inline
T
atomic_add_fetch(T *mem, V value)
{
  T val = value;
  T old;
  asm volatile ("lock; xadd %1, %0" : "+m"(*mem), "=r"(old) : "1"(val));
  return old + val;
}

template<typename T, typename V> inline
T
atomic_exchange(T *mem, V value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  T val = value;

  // processor locking even without explicit LOCK prefix
  asm volatile ("xchg %[val], %[mem]\n"
                : [val] "=r" (val), [mem] "+m" (*mem) : "0" (val));

  return val;
}

template< typename T > ALWAYS_INLINE inline
T
atomic_load(T const *mem)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  T res;
  switch (sizeof(T))
    {
    case 4:
      asm volatile ("movl %[mem], %[res]" : [res] "=r" (res) : [mem] "m" (*mem));
      return res;

    case 8:
      asm volatile ("movq %[mem], %[res]" : [res] "=r" (res) : [mem] "m" (*mem));
      return res;
    }
}

template< typename T, typename V > ALWAYS_INLINE inline
void
atomic_store(T *mem, V value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  T val = value;
  switch (sizeof(T))
    {
    case 4:
      asm volatile ("movl %[val], %[mem]" : [mem] "=m" (*mem) : [val] "ir" (val));
      break;

    case 8:
      asm volatile ("movq %[val], %[mem]" : [mem] "=m" (*mem) : [val] "r" (val));
      break;
    }
}

inline
void
local_atomic_add(Mword *l, Mword value)
{
  asm volatile ("addq %1, %2" : "=m"(*l) : "er"(value), "m"(*l));
}

inline
void
local_atomic_and(Mword *l, Mword mask)
{
  asm volatile ("andq %1, %2" : "=m"(*l) : "er"(mask), "m"(*l));
}

inline
void
local_atomic_or(Mword *l, Mword bits)
{
  asm volatile ("orq %1, %2" : "=m"(*l) : "er"(bits), "m"(*l));
}

// ``unsafe'' stands for no safety according to the size of the given type.
// There are type safe versions of the cas operations in the architecture
// independent part of atomic that use the unsafe versions and make a type
// check.

inline
bool
local_cas_unsafe(Mword *ptr, Mword cmpval, Mword newval)
{
  Mword oldval_ignore, zflag;

  asm volatile ("cmpxchgq %[newval], %[ptr]"
                : "=a"(oldval_ignore), "=@ccz"(zflag)
                : [newval]"r"(newval), [ptr]"m"(*ptr), "a"(cmpval)
                : "memory");

  return zflag;
}

inline
bool
cas_arch(Mword *ptr, Mword cmpval, Mword newval)
{
  Mword oldval_ignore, zflag;

  asm volatile ("lock; cmpxchgq %[newval], %[ptr]"
                : "=a"(oldval_ignore), "=@ccz"(zflag)
                : [newval]"r"(newval), [ptr]"m"(*ptr), "a"(cmpval)
                : "memory");

  return zflag;
}
