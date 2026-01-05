INTERFACE:

#include "types.h"

//---------------------------------------------------------------------------
IMPLEMENTATION:

/**
 * Atomically test and modify memory \c without protection against concurrent
 * writes from other CPUs (\c not SMP-safe). The memory is only written if it
 * contains a dedicated value. For the variant with protection against
 * concurrent writes from other CPUs, \see cas().
 *
 * \param ptr     Pointer to the memory to change.
 * \param oldval  Only write 'newval' if the memory contains this value.
 * \param newval  Write this value if the memory contains 'oldval'.
 * \return True if the memory was written, false otherwise.
 */
template< typename Type > inline
bool
local_cas(Type *ptr, Type oldval, Type newval)
{
  static_assert(sizeof(Type) == sizeof(Mword));

  if constexpr (cxx::is_pointer_v<Type>)
    return local_cas_unsafe(reinterpret_cast<Mword *>(ptr),
                            reinterpret_cast<Mword>(oldval),
                            reinterpret_cast<Mword>(newval));
  else
    return local_cas_unsafe(reinterpret_cast<Mword *>(ptr),
                            static_cast<Mword>(oldval),
                            static_cast<Mword>(newval));
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
local_atomic_change(T *ptr, T mask, T bits)
{
  T old;
  do
    {
      old = *ptr;
    }
  while (!local_cas(ptr, old, (old & mask) | bits));
  return old;
}

//---------------------------------------------------------------------------
IMPLEMENTATION[(ppc32 && !mp) || (sparc && !mp) || (arm && !arm_v6plus)]:

#include <cxx/type_traits>
#include "processor.h"

// Fall-back UP implementations for ppc32, sparc and armv5

template<typename T, typename V> inline NEEDS["processor.h"]
void
atomic_and(T *l, V value)
{
  static_assert(sizeof(T) == 4);
  T val = value;
  Proc::Status s = Proc::cli_save();
  *l &= val;
  Proc::sti_restore(s);
}

template<typename T, typename V> inline NEEDS["processor.h"]
void
atomic_or(T *l, V value)
{
  static_assert(sizeof(T) == 4);
  T val = value;
  Proc::Status s = Proc::cli_save();
  *l |= val;
  Proc::sti_restore(s);
}

template<typename T, typename V> inline NEEDS["processor.h"]
void
atomic_add(T *l, V value)
{
  static_assert(sizeof(T) == 4);
  T val = value;
  Proc::Status s = Proc::cli_save();
  *l += val;
  Proc::sti_restore(s);
}

template<typename T, typename V> inline NEEDS [<cxx/type_traits>, "processor.h"]
ALWAYS_INLINE cxx::enable_if_t<(sizeof(T) == 4), T>
atomic_add_fetch(T *mem, V value)
{
  static_assert(sizeof(T) == 4);
  T val = value;
  Proc::Status s = Proc::cli_save();
  *mem += val;
  T res = *mem;
  Proc::sti_restore(s);
  return res;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

/**
 * Atomically test and modify memory with protection against concurrent writes
 * from other CPUs (SMP-safe). On UP systems, this function is equivalent to
 * \see 'local_cas'.
 *
 * \param ptr     Pointer to the memory to change.
 * \param oldval  Only write 'newval' if the memory contains this value.
 * \param newval  Write this value if the memory contains 'oldval'.
 * \return True if the memory was written, false otherwise.
 */
template< typename T > inline
bool
cas(T *ptr, T oldval, T newval)
{ return local_cas(ptr, oldval, newval); }

//---------------------------------------------------------------------------
IMPLEMENTATION [mp]:

/**
 * Atomically test and modify memory with protection against concurrent writes
 * from other CPUs (SMP-safe).
 *
 * \note The implementation guarantees that the compiler knows that the memory
 *       is clobbered, even if this was caused by a concurrent write from
 *       another CPU.
 *
 * \param ptr     Pointer to the memory to change.
 * \param oldval  Only write 'newval' if the memory contains this value.
 * \param newval  Write this value if the memory contains 'oldval'.
 * \return True if the memory was written, false otherwise.
 */
template< typename T > inline
bool
cas(T *ptr, T oldval, T newval)
{
  static_assert(sizeof(T) == sizeof(Mword));

  if constexpr (cxx::is_pointer_v<T>)
    return cas_arch(reinterpret_cast<Mword*>(ptr),
                    reinterpret_cast<Mword>(oldval),
                    reinterpret_cast<Mword>(newval));
  else
    return cas_arch(reinterpret_cast<Mword*>(ptr),
                    static_cast<Mword>(oldval),
                    static_cast<Mword>(newval));
}
