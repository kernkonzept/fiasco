IMPLEMENTATION[amd64]:

EXTENSION class Io
{
public:
  typedef unsigned short Port_addr;
};

/* This is a more reliable delay than a few short jmps. */
IMPLEMENT inline
void Io::iodelay() 
{
  asm("inb $0x80,%%al; inb $0x80,%%al" : : : "eax");
}

IMPLEMENT inline
Unsigned8  Io::in8 ( unsigned long port )
{
  Unsigned8 tmp;
  asm volatile ("inb %w1, %b0": "=a"(tmp) : "Nd"(port) );
  return tmp;
}

IMPLEMENT inline
Unsigned16 Io::in16( unsigned long port )
{
  Unsigned16 tmp;
  asm volatile ("inw %w1, %w0": "=a"(tmp) : "Nd"(port) );
  return tmp;
}

IMPLEMENT inline
Unsigned32 Io::in32( unsigned long port )
{
  Unsigned32 tmp;
  asm volatile ("in %w1, %0": "=a"(tmp) : "Nd"(port) );
  return tmp;
}

IMPLEMENT inline
void Io::out8 ( Unsigned8  val, unsigned long port )
{
  asm volatile ("outb %b0, %w1": : "a"(val), "Nd"(port) );
}

IMPLEMENT inline
void Io::out16( Unsigned16 val, unsigned long port )
{
  asm volatile ("outw %w0, %w1": : "a"(val), "Nd"(port) );
}

IMPLEMENT inline
void Io::out32( Unsigned32 val, unsigned long port )
{
  asm volatile ("out %0, %w1": : "a"(val), "Nd"(port) );
}


