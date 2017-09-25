INTERFACE [ppc32]:

EXTENSION class Io
{
public:
  /**
   * Read but do not disable data cache
   */
  template< typename T >
  static T read_dirty(Address address);

  /**
   * Write but do not disable data cache
   */
  template< typename T>
  static void write_dirty(T value, Address address);
};


IMPLEMENTATION [ppc32]:

#include "mem_unit.h"
#include "mmu.h"

IMPLEMENT inline NEEDS["mem_unit.h"]
template< typename T >
T Io::read_dirty(Address address)
{
  volatile T ret;
  Mem_unit::sync();
  ret = *(volatile T *)address;
  Mem_unit::isync();
  return ret;
}

IMPLEMENT inline NEEDS["mmu.h"]
template< typename T >
T Io::read(Address address)
{
  Mmu_guard dcache;
  return read_dirty<T>(address);
}

IMPLEMENT inline NEEDS["mem_unit.h"]
template< typename T>
void Io::write_dirty(T value, Address address)
{
  Mem_unit::sync();
  *(volatile T *)address = value;
}

IMPLEMENT inline NEEDS["mmu.h"]
template< typename T>
void Io::write(T value, Address address)
{
  Mmu_guard dcache;
  write_dirty<T>(value, address);
}

//------------------------------------------------------------------------------
/**
 * No port support
 */
IMPLEMENT inline
void Io::iodelay() 
{}

IMPLEMENT inline
Unsigned8  Io::in8 ( unsigned long /* port */ )
{
  return 0;
}

IMPLEMENT inline
Unsigned16 Io::in16( unsigned long /* port */ )
{
  return 0;
}

IMPLEMENT inline
Unsigned32 Io::in32( unsigned long /* port */ )
{
  return 0;
}

IMPLEMENT inline
void Io::out8 ( Unsigned8 /* val */, unsigned long /* port */ )
{
}

IMPLEMENT inline
void Io::out16( Unsigned16 /* val */, unsigned long /* port */ )
{
}

IMPLEMENT inline
void Io::out32( Unsigned32 /* val */, unsigned long /* port */ )
{
}


