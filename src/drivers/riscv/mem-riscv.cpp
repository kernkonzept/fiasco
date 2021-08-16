IMPLEMENTATION [riscv]:

IMPLEMENT static inline
void
Mem::memset_mwords(void *dst, const unsigned long value,
                   unsigned long nr_of_mwords)
{
  unsigned long *d = static_cast<unsigned long *>(dst);
  for (; nr_of_mwords--; d++)
    *d = value;
}

IMPLEMENT static inline
void
Mem::memcpy_mwords(void *dst, void const *src, unsigned long nr_of_mwords)
{
  __builtin_memcpy(dst, src, nr_of_mwords * sizeof(unsigned long));
}

IMPLEMENT static inline
void
Mem::memcpy_bytes(void *dst, void const *src, unsigned long nr_of_bytes)
{
  __builtin_memcpy(dst, src, nr_of_bytes);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [riscv && mp]:

IMPLEMENT inline static
void
Mem::mb()
{ __asm__ __volatile__ ("fence iorw, iorw" : : : "memory"); }

IMPLEMENT inline static
void
Mem::rmb()
{ __asm__ __volatile__ ("fence ir, ir" : : : "memory"); }

IMPLEMENT inline static
void
Mem::wmb()
{ __asm__ __volatile__ ("fence ow, ow" : : : "memory"); }

// XXX Evaluate whether the I/O space really should be part of every mp_*.
// The alternative would be to selectively use I/O barriers in code sections
// that do concurrent I/O.
// For example Irq_chip_sifive::mask()/unmask() vs. Irq_chip_sifive::set_cpu().
// Here we would need an Io_spin_lock that uses I/O variants of mp_acquire()
// and mp_release().
IMPLEMENT inline static
void
Mem::mp_mb()
{ __asm__ __volatile__ ("fence iorw, iorw" : : : "memory"); }

IMPLEMENT inline static
void
Mem::mp_rmb()
{ __asm__ __volatile__ ("fence ir, ir" : : : "memory"); }

IMPLEMENT inline static
void
Mem::mp_wmb()
{ __asm__ __volatile__ ("fence ow, ow" : : : "memory"); }

IMPLEMENT inline static
void
Mem::mp_acquire()
{ __asm__ __volatile__ ("fence ir, iorw" : : : "memory"); }

IMPLEMENT inline static
void
Mem::mp_release()
{ __asm__ __volatile__ ("fence iorw, ow" : : : "memory"); }
