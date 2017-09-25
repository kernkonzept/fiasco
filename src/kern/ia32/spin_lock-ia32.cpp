//---------------------------------------------------------------------------
INTERFACE [(ia32|ux|amd64) && mp]:

EXTENSION class Spin_lock
{
public:
  enum { Arch_lock = 2 };
};

//--------------------------------------------------------------------------
IMPLEMENTATION [(ia32|ux|amd64) && mp]:

PRIVATE template<typename Lock_t> inline
void
Spin_lock<Lock_t>::lock_arch()
{
  Lock_t dummy, tmp;
#define L(z)                       \
  __asm__ __volatile__ (           \
      "1: mov %[lock], %[tmp]  \n" \
      "   test $2, %[tmp]      \n" /* Arch_lock == #2 */ \
      "   jz 2f                \n" \
      "   pause                \n" \
      "   jmp 1b               \n" \
      "2: mov %[tmp], %[d]     \n" \
      "   or $2, %[d]          \n" \
      "   lock; cmpxchg %[d], %[lock]  \n" \
      "   jnz 1b                \n" \
      : [d] "=&"#z (dummy), [tmp] "=&a" (tmp), [lock] "+m" (_lock))

  if (sizeof(Lock_t) > sizeof(char))
    L(r);
  else
    L(q);
#undef L
}

PRIVATE template<typename Lock_t> inline
void
Spin_lock<Lock_t>::unlock_arch()
{
  _lock &= ~Arch_lock;
}
