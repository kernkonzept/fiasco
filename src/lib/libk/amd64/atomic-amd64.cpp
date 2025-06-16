IMPLEMENTATION [amd64]:

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

template<typename T, typename V> inline
void
atomic_and(T *l, V value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  T val = value;
  switch (sizeof(T))
    {
    case 4:
      asm volatile ("lock; andl %1, %2" : "=m"(*l) : "er"(val), "m"(*l));
      break;
    case 8:
      asm volatile ("lock; andq %1, %2" : "=m"(*l) : "er"(val), "m"(*l));
      break;
    }
}

template<typename T, typename V> inline
void
atomic_or(T *l, V value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  T val = value;
  switch (sizeof(T))
    {
    case 4:
      asm volatile ("lock; orl %1, %2" : "=m"(*l) : "er"(val), "m"(*l));
      break;
    case 8:
      asm volatile ("lock; orq %1, %2" : "=m"(*l) : "er"(val), "m"(*l));
      break;
    }
}

template<typename T, typename V> inline
void
atomic_add(T *l, V value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  T val = value;
  switch (sizeof(T))
    {
    case 4:
      asm volatile ("lock; addl %1, %2" : "=m"(*l) : "er"(val), "m"(*l));
      break;
    case 8:
      asm volatile ("lock; addq %1, %2" : "=m"(*l) : "er"(val), "m"(*l));
      break;
    }
}

template<typename T, typename V> inline
T
atomic_add_fetch(T *mem, V value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  T res;
  T val = value;
  asm volatile ("lock; xadd %1, %0" : "+m"(*mem), "=r"(res) : "1"(val));
  return res + val;
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
