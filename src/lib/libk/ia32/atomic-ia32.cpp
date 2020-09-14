IMPLEMENTATION [ia32,ux]:

inline
void
atomic_mp_and(Mword *l, Mword value)
{
  asm volatile ("lock; andl %1, %2" : "=m"(*l) : "ir"(value), "m"(*l));
}

inline
void
atomic_mp_or(Mword *l, Mword value)
{
  asm volatile ("lock; orl %1, %2" : "=m"(*l) : "ir"(value), "m"(*l));
}


inline
void
atomic_mp_add(Mword *l, Mword value)
{
  asm volatile ("lock; addl %1, %2" : "=m"(*l) : "ir"(value), "m"(*l));
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

//---------------------------------------------------------------------------
IMPLEMENTATION [(ia32,ux)&& !cc_has_asm_flag_outputs]:
// ``unsafe'' stands for no safety according to the size of the given type.
// There are type safe versions of the cas operations in the architecture
// independent part of atomic that use the unsafe versions and make a type
// check.

inline
bool
local_cas_unsafe(Mword *ptr, Mword cmpval, Mword newval)
{
  Mword oldval;

  asm volatile ("cmpxchgl %[newval], %[ptr]"
                : "=a"(oldval)
                : [newval]"r"(newval), [ptr]"m"(*ptr), "a"(cmpval)
                : "memory");

  return oldval == cmpval;
}

inline
bool
mp_cas_arch(Mword *ptr, Mword cmpval, Mword newval)
{
  Mword oldval;

  asm volatile ("lock; cmpxchgl %[newval], %[ptr]"
                : "=a"(oldval)
                : [newval]"r"(newval), [ptr]"m"(*ptr), "a"(cmpval)
                : "memory");

  return oldval == cmpval;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [(ia32,ux) && cc_has_asm_flag_outputs]:
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
mp_cas_arch(Mword *ptr, Mword cmpval, Mword newval)
{
  Mword oldval_ignore, zflag;

  asm volatile ("lock; cmpxchgl %[newval], %[ptr]"
                : "=a"(oldval_ignore), "=@ccz"(zflag)
                : [newval]"r"(newval), [ptr]"m"(*ptr), "a"(cmpval)
                : "memory");

  return zflag;
}
