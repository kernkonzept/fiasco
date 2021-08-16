INTERFACE [riscv && mp]:

EXTENSION class Spin_lock
{
public:
  enum { Arch_lock = 2 };
  // Atomic operations can operate on: 32-bit (RV32 and RV64), 64-bit (RV64)
  static_assert(sizeof(Lock_t) == 4 || sizeof(Lock_t) == sizeof(Mword),
                "unsupported spin-lock type for RISC-V");
};

//--------------------------------------------------------------------------
IMPLEMENTATION [riscv && mp]:

PRIVATE template<typename Lock_t> inline
void
Spin_lock<Lock_t>::lock_arch()
{
  Mword prev;
#define LOCK_ARCH(width) \
  __asm__ __volatile__ ( \
    "1:                                            \n" \
    "amoor." #width ".aq %[prev], %[mask], %[lock] \n" \
    "and %[prev], %[prev], %[mask]                 \n" \
    "bnez %[prev], 1b                              \n" \
    : [prev] "=&r" (prev), [lock] "+A"(_lock) \
    : [mask] "r" (Arch_lock) \
    : "memory");

  switch (sizeof(Lock_t))
    {
    case sizeof(Unsigned32): LOCK_ARCH(w); break;
    case sizeof(Unsigned64): LOCK_ARCH(d); break;
    }

#undef LOCK_ARCH
}

PRIVATE template<typename Lock_t> inline NEEDS["mem.h"]
void
Spin_lock<Lock_t>::unlock_arch()
{
  Mword prev;
#define UNLOCK_ARCH(width, type) \
  __asm__ __volatile__ ( \
    "amoand." #width " %[prev], %[mask], %[lock]" \
    : [prev] "=r" (prev), [lock] "+A"(_lock) \
    : [mask] "r" (~static_cast<type>(Arch_lock)) \
    : "memory");

  switch (sizeof(Lock_t))
    {
    case sizeof(Unsigned32): UNLOCK_ARCH(w, Unsigned32); break;
    case sizeof(Unsigned64): UNLOCK_ARCH(d, Unsigned64); break;
    }

#undef UNLOCK_ARCH
}
