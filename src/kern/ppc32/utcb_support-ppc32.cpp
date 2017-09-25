//-------------------------------------------------------------------------
IMPLEMENTATION [ppc32]:

IMPLEMENT inline
void
Utcb_support::current(User<Utcb>::Ptr const &utcb)
{
  asm volatile ("mr %%r2, %0" : : "r" (utcb.get()) : "memory");
}
