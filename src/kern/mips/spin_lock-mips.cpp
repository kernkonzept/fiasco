INTERFACE [mips && mp]:

EXTENSION class Spin_lock
{
public:
  enum { Arch_lock = 2 };
  static_assert(sizeof(Lock_t) >= sizeof(Unsigned32),
                "unsupported spin-lock type for MIPS");
};

//--------------------------------------------------------------------------
IMPLEMENTATION [mips && mp]:

PRIVATE template<typename Lock_t> inline
void
Spin_lock<Lock_t>::lock_arch()
{
  Lock_t dummy, tmp;

  __asm__ __volatile__(
      "   .set    push            \n"
      "   .set    noreorder       \n"
      "1: lw      %[d], %[lock]   \n"
      "   andi    %[d], %[d], 2   \n"
      "   bnez    %[d], 1b        \n"
      "     nop                   \n"
      "   ll      %[d], %[lock]   \n"
      "   andi    %[tmp], %[d], 2 \n" /* Arch_lock == #2 */
      "   bnez    %[tmp], 1b      \n" /* branch if lock already taken */
      "     ori   %[d], %[d], 2   \n" /* Arch_lock == #2 */
      "   sc      %[d], %[lock]   \n" /* acquire lock atomically */
      "   beqz    %[d], 1b        \n" /* branch if failed */
      "     nop                  \n"
      "   .set    pop             \n"
      : [lock] "+m" (_lock), [tmp] "=&r" (tmp), [d] "=&r" (dummy)
      : : "memory");
}

PRIVATE template<typename Lock_t> inline NEEDS["mem.h"]
void
Spin_lock<Lock_t>::unlock_arch()
{
  Mem::mp_mb();
  _lock &= ~Arch_lock;
}
