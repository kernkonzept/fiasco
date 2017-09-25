IMPLEMENTATION [amd64]:

inline
void
atomic_mp_and(Mword *l, Mword value)
{
  asm volatile ("lock; andq %1, %2" : "=m"(*l) : "ir"(value), "m"(*l));
}

inline
void
atomic_mp_or(Mword *l, Mword value)
{
  asm volatile ("lock; orq %1, %2" : "=m"(*l) : "ir"(value), "m"(*l));
}

inline
void
atomic_mp_add (Mword *l, Mword value)
{
  asm volatile ("lock; addq %1, %2" : "=m"(*l) : "ir"(value), "m"(*l));
}

inline
void
atomic_add (Mword *l, Mword value)
{
  asm volatile ("addq %1, %2" : "=m"(*l) : "ir"(value), "m"(*l));
}

inline
void
atomic_and (Mword *l, Mword mask)
{
  asm volatile ("andq %1, %2" : "=m"(*l) : "ir"(mask), "m"(*l));
}

inline
void
atomic_or (Mword *l, Mword bits)
{
  asm volatile ("orq %1, %2" : "=m"(*l) : "ir"(bits), "m"(*l));
}

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
    ("cmpxchgq %1, %2"
     : "=a" (tmp)
     : "r" (newval), "m" (*ptr), "a" (oldval)
     : "memory");

  return tmp == oldval;
}

inline
bool
mp_cas_arch (Mword *ptr, Mword oldval, Mword newval)
{
  Mword tmp;

  asm volatile
    ("lock; cmpxchgq %1, %2"
     : "=a" (tmp)
     : "r" (newval), "m" (*ptr), "a" (oldval)
     : "memory");

  return tmp == oldval;
}

inline
bool
cas2_unsafe (Mword *ptr, Mword *oldval, Mword *newval)
{
  char ret;
  asm volatile
    ("cmpxchg16b %3 ; sete %%cl"
     : "=c" (ret), 
       "=a" (* oldval), 
       "=d" (*(oldval+1))
     : "m" (*ptr) , 
       "a" (* oldval), "d" (*(oldval+1)), 
       "b" (* newval), "c" (*(newval+1))
     : "memory");

  return ret;
}

inline
bool
mp_cas2_arch (char *p, Mword o1, Mword o2, Mword n1, Mword n2)
{
  char ret;
  asm volatile
    ("lock; cmpxchg16b %3 ; sete %%cl"
     : "=c" (ret), "=a" (o1), "=d" (o2)
     : "m" (*p), "a" (o1), "d" (o2),
       "b" (n1), "c" (n2)
     : "memory");

  return ret;
}
