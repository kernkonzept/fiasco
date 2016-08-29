IMPLEMENTATION[arm]:

inline
void
atomic_mp_add(Mword *l, Mword value)
{
  Mword tmp, ret;

  asm volatile (
      "1:                                 \n"
      "ldxr    %[v], [%[mem]]             \n"
      "add     %[v], %[v], %[addval]      \n"
      "stxr    %w[ret], %[v], [%[mem]]    \n"
      "cbnz    %w[ret], 1b                \n"
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
      "ldxr    %[v], [%[mem]]             \n"
      "and     %[v], %[v], %[andval]      \n"
      "stxr    %w[ret], %[v], [%[mem]]    \n"
      "cbnz    %w[ret], 1b                \n"
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
      "ldxr    %[v], [%[mem]]             \n"
      "orr     %[v], %[v], %[orval]       \n"
      "stxr    %w[ret], %[v], [%[mem]]    \n"
      "cbnz    %w[ret], 1b                \n"
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
     "cmp     %[tmp], %[o]         \n"
     "b.ne    2f                   \n"
     "ldxr    %[tmp], [%[m]]       \n"
     "cmp     %[tmp], %[o]         \n"
     "b.ne    2f                   \n"
     "stxr    %w[res], %[n], [%[m]]\n"
     "cbnz    %w[res], 1b          \n"
     "2:                           \n"
     : [tmp] "=&r" (tmp), [res] "=&r" (res), "+m" (*m)
     : [n] "r" (n), [m] "r" (m), [o] "r" (o)
     : "cc");

  // res == 0 is ok
  // res == 1 is failed

  return !res;
}

