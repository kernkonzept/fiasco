/**
 * Functions to access user address space from inside the kernel.
 */

INTERFACE:

EXTENSION class Mem_space
{
public:
  /**
   * Read integral type at any virtual address.
   * @param addr Virtual address in user or kernel address space.
   * @param user_space Location of virtual address (user or kernel space).
   * @return Integral value read from address.
   */
  template< typename T >
  T peek(T const *addr, bool user_space);

  /**
   * Read integral type at virtual address in user space.
   * @param addr Virtual address in user address space.
   * @return Integral value read from address.
   */
  template< typename T >
  T peek_user(T const *addr);

  /**
   * Set integral type at virtual address in user space to value.
   * @param addr Virtual address in user address space.
   * @param value New value to be set.
   */
  template< typename T >
  void poke_user(T *addr, T value);
};

//----------------------------------------------------------------------------
IMPLEMENTATION:

#include <cassert>
#include "mem.h"

IMPLEMENT inline
template< typename T >
T
Mem_space::peek(T const *addr, bool user_space)
{
  return user_space ? peek_user(addr) : *addr;
}

//----------------------------------------------------------------------------
IMPLEMENTATION[arm || ia32 || amd64]:

IMPLEMENT_DEFAULT inline
template< typename T >
T
Mem_space::peek_user(T const *addr)
{
  return *addr;
}

IMPLEMENT inline
template< typename T >
void
Mem_space::poke_user(T *addr, T value)
{
  *addr = value;
}
