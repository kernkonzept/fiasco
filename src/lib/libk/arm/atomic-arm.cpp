IMPLEMENTATION [arm && arm_v6plus]:

inline
void
atomic_or(Mword *l, Mword value)
{ atomic_mp_or(l, value); }

inline bool
cas_unsafe(Mword *ptr, Mword oldval, Mword newval)
{ return mp_cas_arch(ptr, oldval, newval); }

inline
void
atomic_and(Mword *l, Mword mask)
{ atomic_mp_and(l, mask); }

