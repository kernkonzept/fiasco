IMPLEMENTATION[arm]:

inline
void
atomic_mp_add(Mword *l, Mword value)
{
  Mword tmp, ret;

  asm volatile (
      "1:                             \n"
      "ldxr    %[v], %[l]             \n"
      "add     %[v], %[v], %[addval]  \n"
      "stxr    %w[ret], %[v], %[l]    \n"
      "cbnz    %w[ret], 1b            \n"
      : [v] "=&r" (tmp), [ret] "=&r" (ret), [l] "+Q" (*l)
      : [addval] "r" (value)
      : "cc");
}

inline
void
atomic_mp_and(Mword *l, Mword value)
{
  Mword tmp, ret;

  asm volatile (
      "1:                             \n"
      "ldxr    %[v], %[l]             \n"
      "and     %[v], %[v], %[andval]  \n"
      "stxr    %w[ret], %[v], %[l]    \n"
      "cbnz    %w[ret], 1b            \n"
      : [v] "=&r" (tmp), [ret] "=&r" (ret), [l] "+Q" (*l)
      : [andval] "r" (value)
      : "cc");
}

inline
void
atomic_mp_or(Mword *l, Mword value)
{
  Mword tmp, ret;

  asm volatile (
      "1:                             \n"
      "ldxr    %[v], %[l]             \n"
      "orr     %[v], %[v], %[orval]   \n"
      "stxr    %w[ret], %[v], %[l]    \n"
      "cbnz    %w[ret], 1b            \n"
      : [v] "=&r" (tmp), [ret] "=&r" (ret), [l] "+Q" (*l)
      : [orval] "r" (value)
      : "cc");
}

inline
bool
mp_cas_arch(Mword *m, Mword o, Mword n)
{
  Mword tmp, res;

  asm volatile
    ("mov     %[res], #1          \n"
     "prfm    pstl1strm, %[m]     \n"
     "1:                          \n"
     "ldxr    %[tmp], %[m]        \n"
     "cmp     %[tmp], %[o]        \n"
     "b.ne    2f                  \n"
     "stxr    %w[res], %[n], %[m] \n"
     "cbnz    %w[res], 1b         \n"
     "dmb     ish                 \n"
     "2:                          \n"
     : [tmp] "=&r" (tmp), [res] "=&r" (res), [m] "+Q" (*m)
     : [n] "r" (n), [o] "r" (o)
     : "cc", "memory");

  // res == 0 is ok
  // res == 1 is failed

  return !res;
}

