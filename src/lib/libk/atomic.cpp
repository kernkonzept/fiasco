INTERFACE:

#include "types.h"

extern "C" void cas_error_type_with_bad_size_used(void);

#define MACRO_CAS_ASSERT(rs, cs) \
  if ((rs) != (cs)) \
    cas_error_type_with_bad_size_used()


template< typename A, typename B >
struct Pair
{
  A first;
  B second;

  Pair() {}
  Pair(A const &a, B const &b) : first(a), second(b) {}
};


//---------------------------------------------------------------------------
IMPLEMENTATION:

template< typename Type > inline
bool
cas(Type *ptr, Type oldval, Type newval)
{
  MACRO_CAS_ASSERT(sizeof(Type), sizeof(Mword));
  return cas_unsafe(reinterpret_cast<Mword *>(ptr),
                    (Mword)oldval, (Mword)newval);
}


template <typename T> inline
T
atomic_change(T *ptr, T mask, T bits)
{
  T old;
  do
    {
      old = *ptr;
    }
  while (!cas(ptr, old, (old & mask) | bits));
  return old;
}

//---------------------------------------------------------------------------
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
atomic_add(Mword *l, Mword value)
{
  asm volatile ("addl %1, %2" : "=m"(*l) : "ir"(value), "m"(*l));
}

inline
void
atomic_and(Mword *l, Mword mask)
{
  asm volatile ("andl %1, %2" : "=m"(*l) : "ir"(mask), "m"(*l));
}

inline
void
atomic_or(Mword *l, Mword bits)
{
  asm volatile ("orl %1, %2" : "=m"(*l) : "ir"(bits), "m"(*l));
}

// ``unsafe'' stands for no safety according to the size of the given type.
// There are type safe versions of the cas operations in the architecture
// independent part of atomic that use the unsafe versions and make a type
// check.

inline
bool
cas_unsafe(Mword *ptr, Mword oldval, Mword newval)
{
  Mword tmp;

  asm volatile
    ("cmpxchgl %1, %2"
     : "=a" (tmp)
     : "r" (newval), "m" (*ptr), "a" (oldval)
     : "memory");

  return tmp == oldval;
}


inline
bool
mp_cas_arch(Mword *m, Mword o, Mword n)
{
  Mword tmp;

  asm volatile
    ("lock; cmpxchgl %1, %2"
     : "=a" (tmp)
     : "r" (n), "m" (*m), "a" (o)
     : "memory");

  return tmp == o;
}

//---------------------------------------------------------------------------
IMPLEMENTATION[(ppc32 && !mp) || (sparc && !mp) || (arm && (!armv6plus || arm1136))]:

#include "processor.h"

inline NEEDS["processor.h"]
void
atomic_mp_and(Mword *l, Mword value)
{
  Proc::Status s = Proc::cli_save();
  *l &= value;
  Proc::sti_restore(s);
}

inline NEEDS["processor.h"]
void
atomic_mp_or(Mword *l, Mword value)
{
  Proc::Status s = Proc::cli_save();
  *l |= value;
  Proc::sti_restore(s);
}

inline NEEDS["processor.h"]
void
atomic_mp_add(Mword *l, Mword value)
{
  Proc::Status s = Proc::cli_save();
  *l += value;
  Proc::sti_restore(s);
}

//---------------------------------------------------------------------------
IMPLEMENTATION[arm && armv6plus && !arm1136]:

inline
void
atomic_mp_add(Mword *l, Mword value)
{
  Mword tmp, ret;

  asm volatile (
      "1:                                 \n"
      "ldrex   %[v], [%[mem]]             \n"
      "add     %[v], %[v], %[addval]      \n"
      "strex   %[ret], %[v], [%[mem]]     \n"
      "teq     %[ret], #0                 \n"
      "bne     1b                         \n"
      : [v] "=&r" (tmp), [ret] "=&r" (ret), "+m" (*l)
      :  [mem] "r" (l), [addval] "r" (value)
      : "cc");
}

inline
void
atomic_mp_and(Mword *l, Mword value)
{
  Mword tmp, ret;

  asm volatile (
      "1:                                 \n"
      "ldrex   %[v], [%[mem]]             \n"
      "and     %[v], %[v], %[andval]     \n"
      "strex   %[ret], %[v], [%[mem]]     \n"
      "teq     %[ret], #0                 \n"
      "bne     1b                         \n"
      : [v] "=&r" (tmp), [ret] "=&r" (ret), "+m" (*l)
      :  [mem] "r" (l), [andval] "r" (value)
      : "cc");
}

inline
void
atomic_mp_or(Mword *l, Mword value)
{
  Mword tmp, ret;

  asm volatile (
      "1:                                 \n"
      "ldrex   %[v], [%[mem]]             \n"
      "orr     %[v], %[v], %[orval]     \n"
      "strex   %[ret], %[v], [%[mem]]     \n"
      "teq     %[ret], #0                 \n"
      "bne     1b                         \n"
      : [v] "=&r" (tmp), [ret] "=&r" (ret), "+m" (*l)
      :  [mem] "r" (l), [orval] "r" (value)
      : "cc");
}


inline
bool
mp_cas_arch(Mword *m, Mword o, Mword n)
{
  Mword tmp, res;

  asm volatile
    ("mov     %[res], #1           \n"
     "1:                           \n"
     "ldr     %[tmp], [%[m]]       \n"
     "teq     %[tmp], %[o]         \n"
     "bne     2f                   \n"
     "ldrex   %[tmp], [%[m]]       \n"
     "teq     %[tmp], %[o]         \n"
     "strexeq %[res], %[n], [%[m]] \n"
     "teq     %[res], #1           \n"
     "beq     1b                   \n"
     "2:                           \n"
     : [tmp] "=&r" (tmp), [res] "=&r" (res), "+m" (*m)
     : [n] "r" (n), [m] "r" (m), [o] "r" (o)
     : "cc");

  // res == 0 is ok
  // res == 1 is failed

  return !res;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [mp]:

template< typename T > inline
bool
mp_cas(T *m, T o, T n)
{
  MACRO_CAS_ASSERT(sizeof(T),sizeof(Mword));
  return mp_cas_arch(reinterpret_cast<Mword*>(m),
                     Mword(o),
                     Mword(n));
}

//---------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

template< typename T > inline
bool
mp_cas(T *m, T o, T n)
{ return cas(m,o,n); }

