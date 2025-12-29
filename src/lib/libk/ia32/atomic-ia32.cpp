IMPLEMENTATION [ia32]:

template<typename T, typename V> inline
T
atomic_exchange(T *mem, V value)
{
  T val = value;

  // processor locking even without explicit LOCK prefix
  asm volatile ("xchg %[val], %[mem]\n"
                : [val] "=r" (val), [mem] "+m" (*mem) : "0" (val));

  return val;
}

template<typename T, typename V> inline
void
atomic_and(T *l, V value)
{
  static_assert(sizeof(T) == 4);
  T val = value;
  asm volatile ("lock; andl %1, %2" : "=m"(*l) : "ir"(val), "m"(*l));
}

template<typename T, typename V> inline
void
atomic_or(T *l, V value)
{
  static_assert(sizeof(T) == 4);
  T val = value;
  asm volatile ("lock; orl %1, %2" : "=m"(*l) : "ir"(val), "m"(*l));
}

template<typename T, typename V> inline
void
atomic_add(T *l, V value)
{
  static_assert(sizeof(T) == 4);
  T val = value;
  asm volatile ("lock; addl %1, %2" : "=m"(*l) : "ir"(val), "m"(*l));
}

template<typename T> inline
T
atomic_add_fetch(T *mem, T addend)
{
  static_assert(sizeof(T) == 4);
  T res;
  asm volatile ("lock; xadd %1, %0" : "+m"(*mem), "=r"(res) : "1"(addend));
  return res + addend;
}

template< typename T > ALWAYS_INLINE inline
T
atomic_load(T const *mem)
{
  static_assert(sizeof(T) == 4);
  T res;
  asm volatile ("movl %[mem], %[res]" : [res] "=r" (res) : [mem] "m" (*mem));
  return res;
}

template< typename T, typename V > ALWAYS_INLINE inline
void
atomic_store(T *mem, V value)
{
  static_assert(sizeof(T) == 4);
  T val = value;
  asm volatile ("movl %[val], %[mem]" : [mem] "=m" (*mem) : [val] "ir" (val));
}

inline
void
local_atomic_add(Mword *l, Mword value)
{
  asm volatile ("addl %1, %2" : "=m"(*l) : "ir"(value), "m"(*l));
}

inline
void
local_atomic_and(Mword *l, Mword mask)
{
  asm volatile ("andl %1, %2" : "=m"(*l) : "ir"(mask), "m"(*l));
}

inline
void
local_atomic_or(Mword *l, Mword bits)
{
  asm volatile ("orl %1, %2" : "=m"(*l) : "ir"(bits), "m"(*l));
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

  asm volatile ("cmpxchgl %[newval], %[ptr]"
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

  asm volatile ("lock; cmpxchgl %[newval], %[ptr]"
                : "=a"(oldval_ignore), "=@ccz"(zflag)
                : [newval]"r"(newval), [ptr]"m"(*ptr), "a"(cmpval)
                : "memory");

  return zflag;
}
