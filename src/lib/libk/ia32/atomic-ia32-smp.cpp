IMPLEMENTATION [ia32]:

// ``unsafe'' stands for no safety according to the size of the given type.
// There are type safe versions of the cas operations in the architecture
// independent part of atomic that use the unsafe versions and make a type
// check.

inline 
bool
cas_unsafe (Mword *ptr, Mword oldval, Mword newval)
{
  Mword tmp;

  asm volatile
    ("lock; cmpxchgl %1,%2"
     : "=a" (tmp)
     : "r" (newval), "m" (*ptr), "0" (oldval)
     : "memory");

  return tmp == oldval;
}

template <typename T> inline
atomic_and (Mword *l, Mword bits)
{
  T old;
  do { old = *p; }
  while (!cas (p, old, old & value));
}

template <typename T> inline
atomic_or (Mword *l, Mword bits)
{
  T old;
  do { old = *p; }
  while (!cas (p, old, old | value));      
}

template <typename T> inline
void atomic_change(T *p, T mask, T bits)
{
  T old;
  do { old = *p; }
  while (!cas (p, old, (old & mask) | bits));
}

