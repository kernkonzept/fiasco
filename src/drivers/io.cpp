INTERFACE:

#include "types.h"

/**
 * I/O API.
 */
class Io
{
public:

  /// Delay for slow I/O operations.
  static void iodelay();

  /**
   * Read the value of type T at address.
   */
  template< typename T >
  static T read(Address address);

  /**
   * Write the value of type T at address.
   */
  template< typename T >
  static void write(T value, Address address);

  /**
   * Write (read<T>(address) & maskbits) of type T at address.
   */
  template< typename T >
  static void mask(T mask, Address address);

  /**
   * Write (read<T>(address) & ~clearbits) of type T at address.
   */
  template< typename T >
  static void clear(T clearbits, Address address);

  /**
   * Write (read<T>(address) | setbits) of type T at address.
   */
  template< typename T >
  static void set(T setbits, Address address);

  /**
   * Write ((read<T>(address) & ~disable) | enable) of type T at address.
   */
  template< typename T >
  static void modify(T enable, T disable, Address address);

  /**
   * Read byte port.
   */
  static Unsigned8 in8(unsigned long port);

  /**
   * Read 16-bit port.
   */
  static Unsigned16 in16(unsigned long port);

  /**
   * Read 32-bit port.
   */
  static Unsigned32 in32(unsigned long port);

  /**
   * Write byte port.
   */
  static void out8(Unsigned8 val, unsigned long port);

  /**
   * Write 16-bit port.
   */
  static void out16(Unsigned16 val, unsigned long port);

  /**
   * Write 32-bit port.
   */
  static void out32(Unsigned32 val, unsigned long port);

  /// @name Delayed versions.
  //@{

  /**
   * Read 8-bit port.
   */
  static Unsigned8 in8_p(unsigned long port);

  /**
   * Read 16-bit port.
   */
  static Unsigned16 in16_p(unsigned long port);

  /**
   * Read 32-bit port.
   */
  static Unsigned32 in32_p(unsigned long port);

  /**
   * Write 8-bit port.
   */
  static void out8_p(Unsigned8 val, unsigned long port);

  /**
   * Write 16-bit port.
   */
  static void out16_p(Unsigned16 val, unsigned long port);

  /**
   * Write 32-bit port.
   */
  static void out32_p(Unsigned32 val, unsigned long port);
  //@}
};

// ----------------------------------------------------------------------
IMPLEMENTATION [!ppc32]:

IMPLEMENT inline
template< typename T >
T Io::read(Address address)
{ return *(volatile T *)address; }

IMPLEMENT inline
template< typename T>
void Io::write(T value, Address address)
{ *(volatile T *)address = value; }

// ----------------------------------------------------------------------
IMPLEMENTATION:

IMPLEMENT inline
template< typename T>
void Io::mask(T mask, Address address)
{ write<T>(read<T>(address) & mask, address); }

IMPLEMENT inline
template< typename T>
void Io::clear(T clearbits, Address address)
{ write<T>(read<T>(address) & ~clearbits, address); }

IMPLEMENT inline
template< typename T>
void Io::set(T setbits, Address address)
{ write<T>(read<T>(address) | setbits, address); }

IMPLEMENT inline
template< typename T>
void Io::modify(T enable, T disable, Address address)
{
  write<T>((read<T>(address) & ~disable) | enable, address);
}



IMPLEMENT inline
Unsigned8 Io::in8_p(unsigned long port)
{
  Unsigned8 tmp = in8(port);
  iodelay();
  return tmp;
}

IMPLEMENT inline
Unsigned16 Io::in16_p(unsigned long port)
{
  Unsigned16 tmp = in16(port);
  iodelay();
  return tmp;
}

IMPLEMENT inline
Unsigned32 Io::in32_p(unsigned long port)
{
  Unsigned32 tmp = in32(port);
  iodelay();
  return tmp;
}

IMPLEMENT inline
void Io::out8_p(Unsigned8 val, unsigned long port)
{
  out8(val, port);
  iodelay();
}

IMPLEMENT inline
void Io::out16_p(Unsigned16 val, unsigned long port)
{
  out16(val, port);
  iodelay();
}

IMPLEMENT inline
void Io::out32_p(Unsigned32 val, unsigned long port)
{
  out32(val, port);
  iodelay();
}
