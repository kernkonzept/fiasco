INTERFACE:

#include "types.h"

extern "C" void cas_error_type_with_bad_size_used(void);

#define MACRO_CAS_ASSERT(rs, cs) \
  if ((rs) != (cs)) \
    cas_error_type_with_bad_size_used()


//---------------------------------------------------------------------------
IMPLEMENTATION:

/**
 * Atomically test and modify memory \c without protection against concurrent
 * writes from other CPUs (\c not SMP-safe). The memory is only written if it
 * contains a dedicated value. For the variant with protection against
 * concurrent writes from other CPUs, \see mp_cas().
 *
 * \param ptr     Pointer to the memory to change.
 * \param oldval  Only write 'newval' if the memory contains this value.
 * \param newval  Write this value if the memory contains 'oldval'.
 * \return True if the memory was written, false otherwise.
 */
template< typename Type > inline
bool
cas(Type *ptr, Type oldval, Type newval)
{
  MACRO_CAS_ASSERT(sizeof(Type), sizeof(Mword));
  return cas_unsafe(reinterpret_cast<Mword *>(ptr),
                    (Mword)oldval, (Mword)newval);
}

/**
 * Atomically change certain bits of memory \c without protection against
 * concurrent writes from other CPUs (\c not SMP-safe).
 *
 * \param ptr   Pointer to the memory to change.
 * \param mask  Mask to apply before changing the memory.
 * \param bits  Bits to apply by logical OR to the memory.
 */
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
IMPLEMENTATION[(ppc32 && !mp) || (sparc && !mp) || (arm && !arm_v6plus)]:

#include "processor.h"

// Fall-back UP implementations for ppc32, sparc and armv5

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
IMPLEMENTATION [!mp]:

/**
 * Atomically test and modify memory with protection against concurrent writes
 * from other CPUs (SMP-safe). On UP systems, this function is equivalent to
 * \see 'cas'.
 *
 * \param ptr     Pointer to the memory to change.
 * \param oldval  Only write 'newval' if the memory contains this value.
 * \param newval  Write this value if the memory contains 'oldval'.
 * \return True if the memory was written, false otherwise.
 */
template< typename T > inline
bool
mp_cas(T *ptr, T oldval, T newval)
{ return cas(ptr, oldval, newval); }

//---------------------------------------------------------------------------
IMPLEMENTATION [mp]:

/**
 * Atomically test and modify memory with protection against concurrent writes
 * from other CPUs (SMP-safe).
 *
 * \param ptr     Pointer to the memory to change.
 * \param oldval  Only write 'newval' if the memory contains this value.
 * \param newval  Write this value if the memory contains 'oldval'.
 * \return True if the memory was written, false otherwise.
 */
template< typename T > inline
bool
mp_cas(T *ptr, T oldval, T newval)
{
  MACRO_CAS_ASSERT(sizeof(T),sizeof(Mword));
  return mp_cas_arch(reinterpret_cast<Mword*>(ptr),
                     Mword(oldval),
                     Mword(newval));
}
