INTERFACE:

#include "types.h"

template<typename T, typename V> inline
void
atomic_and(T *mem, V value);

template<typename T, typename V> inline
void
atomic_or(T *mem, V value);

template<typename T, typename V> inline
void
atomic_add(T *mem, V value);

template<typename T, typename V> inline
T
atomic_fetch_and(T *mem, V value);

template<typename T, typename V> inline
T
atomic_fetch_or(T *mem, V value);

template<typename T, typename V> inline
T
atomic_fetch_add(T *mem, V value);

template<typename T, typename V> inline
T
atomic_and_fetch(T *mem, V value);

template<typename T, typename V> inline
T
atomic_or_fetch(T *mem, V value);

template<typename T, typename V> inline
T
atomic_add_fetch(T *mem, V value);

void
local_atomic_and(Mword *mem, Mword value);

void
local_atomic_or(Mword *mem, Mword value);

void
local_atomic_add(Mword *mem, Mword value);

// ``unsafe'' stands for no safety according to the size of the given type.
// There are type safe versions of the cas operations in the architecture
// independent part of atomic that use the unsafe versions and make a type
// check.
bool
local_cas_unsafe(Mword *ptr, Mword oldval, Mword newval);

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

//---------------------------------------------------------------------------
IMPLEMENTATION [generic_local_atomic]:

inline
void
local_atomic_and(Mword *mem, Mword value)
{
  Mword old;
  do { old = *mem; }
  while (!local_cas(mem, old, old & value));
}

inline
void
local_atomic_or(Mword *mem, Mword value)
{
  Mword old;
  do { old = *mem; }
  while (!local_cas(mem, old, old | value));
}

inline
void
local_atomic_add(Mword *mem, Mword value)
{
  Mword old;
  do { old = *mem; }
  while (!local_cas(mem, old, old + value));
}

//---------------------------------------------------------------------------
INTERFACE[generic_atomic && !mp]:

#include "processor.h"

// preprocess off
// Fall-back UP implementations for ppc32, sparc and armv5
#define ATOMIC_OP(name, op)                                                    \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == 4) inline                                              \
  void                                                                         \
  atomic_##name(T *mem, V value)                                               \
  {                                                                            \
    T val = value;                                                             \
    Proc::Status s = Proc::cli_save();                                         \
    *mem op##= val;                                                            \
    Proc::sti_restore(s);                                                      \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == 4) inline                                              \
  T                                                                            \
  atomic_##name##_fetch(T *mem, V value)                                       \
  {                                                                            \
    T val = value;                                                             \
    Proc::Status s = Proc::cli_save();                                         \
    *mem op##= val;                                                            \
    T res = *mem;                                                              \
    Proc::sti_restore(s);                                                      \
    return res;                                                                \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == 4) inline                                              \
  T                                                                            \
  atomic_fetch_##name(T *mem, V value)                                         \
  {                                                                            \
    Proc::Status s = Proc::cli_save();                                         \
    T res = *mem;                                                              \
    *mem op##= value;                                                          \
    Proc::sti_restore(s);                                                      \
    return res;                                                                \
  }
ATOMIC_OP(and, &)
ATOMIC_OP(or, |)
ATOMIC_OP(add, +)
#undef ATOMIC_OP
// preprocess on
