INTERFACE:

#include "console.h"

/**
 * Platform independent UART stub.
 */
class Uart : public Console
{
public:
  /**
   * Type UART transfer mode (Bits, Stopbits etc.).
   */
  typedef unsigned TransferMode;

  /**
   * Type for baud rate.
   */
  typedef unsigned BaudRate;

  /* These constants must be defined in the architecture part of the Uart. To
   * define them there has the advantage of most efficient definition for the
   * hardware.

  static unsigned const PAR_NONE = xxx;
  static unsigned const PAR_EVEN = xxx;
  static unsigned const PAR_ODD  = xxx;
  static unsigned const DAT_5    = xxx;
  static unsigned const DAT_6    = xxx;
  static unsigned const DAT_7    = xxx;
  static unsigned const DAT_8    = xxx;
  static unsigned const STOP_1   = xxx;
  static unsigned const STOP_2   = xxx;

  static unsigned const MODE_8N1 = PAR_NONE | DAT_8 | STOP_1;
  static unsigned const MODE_7E1 = PAR_EVEN | DAT_7 | STOP_1;

  // these two values are to leave either mode
  // or baud rate unchanged on a call to change_mode
  static unsigned const MODE_NC  = xxx;
  static unsigned const BAUD_NC  = xxx;

  */


public:
  /* Interface definition - implemented in the arch part */
  /// ctor
  Uart();

  /// dtor
  ~Uart();

  /**
   * (abstract) Shutdown the serial port.
   */
  void shutdown();

  /**
   * (abstract) Get the IRQ assigned to the port.
   */
  int irq() const;

  /**
   * (abstract) Acknowledge UART interrupt.
   */
  void irq_ack();

  Address base() const;

  /**
   * (abstract) Enable rcv IRQ in UART.
   */
  void enable_rcv_irq();

  /**
   * (abstract) Disable rcv IRQ in UART.
   */
  void disable_rcv_irq();

  /**
   * (abstract) Change transfer mode or speed.
   * @param m the new mode for the transfer, or MODE_NC for no mode change.
   * @param r the new baud rate, or BAUD_NC, for no speed change.
   */
  bool change_mode(TransferMode m, BaudRate r);

  /**
   * (abstract) Get the current transfer mode.
   */
  TransferMode get_mode();

  /**
   * (abstract) Write str.
   */
  int write(char const *str, size_t len) override;

  /**
   * (abstract) Read a character.
   */
  int getchar(bool blocking = true) override;

  /**
   * (abstract) Is there anything to read?
   */
  int char_avail() const override;

  Mword get_attributes() const override;
};

INTERFACE[uart_checksum]:

#include "stream_crc32.h"

EXTENSION class Uart
{
private:
  void checksum_tag();

  /**
   * Current data checksum.
   */
  Stream_crc32 crc32;
};

IMPLEMENTATION:

IMPLEMENT
Mword
Uart::get_attributes() const
{
  return UART | IN | OUT;
}

//---------------------------------------------------------------------------
INTERFACE [libuart]:

#include "uart_base.h"

EXTENSION class Uart
{
public:
  enum
  {
    PAR_NONE = 0x00,
    PAR_EVEN = 0x18,
    PAR_ODD  = 0x08,
    DAT_5    = 0x00,
    DAT_6    = 0x01,
    DAT_7    = 0x02,
    DAT_8    = 0x03,
    STOP_1   = 0x00,
    STOP_2   = 0x04,

    MODE_8N1 = PAR_NONE | DAT_8 | STOP_1,
    MODE_7E1 = PAR_EVEN | DAT_7 | STOP_1,

    // these two values are to leave either mode
    // or baud rate unchanged on a call to change_mode
    MODE_NC  = 0x1000000,
    BAUD_NC  = 0x1000000,
  };

  static L4::Uart *uart();
};


//---------------------------------------------------------------------------
IMPLEMENTATION [libuart]:

IMPLEMENT inline Uart::~Uart()
{}

IMPLEMENT inline void Uart::shutdown()
{
  del_state(ENABLED);
  uart()->shutdown();
}

IMPLEMENT inline bool Uart::change_mode(TransferMode m, BaudRate baud)
{
  return uart()->change_mode(m, baud);
}

IMPLEMENT_DEFAULT inline
int Uart::write(const char *s, __SIZE_TYPE__ count)
{
  return uart()->write(s, count);
}

IMPLEMENT inline
int Uart::getchar(bool blocking)
{
  return uart()->get_char(blocking);
}


IMPLEMENT inline
int Uart::char_avail() const
{
  return uart()->char_avail();
}

IMPLEMENT
void Uart::enable_rcv_irq()
{
  uart()->enable_rx_irq(true);
}

IMPLEMENT
void Uart::disable_rcv_irq()
{
  uart()->enable_rx_irq(false);
}

IMPLEMENT
void Uart::irq_ack()
{
  uart()->irq_ack();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [!libuart]:

IMPLEMENT_DEFAULT
void Uart::irq_ack()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [uart_checksum]:

/**
 * Output the current data checksum tag.
 *
 * The checksum tag size is 12 characters and it has the format:
 *
 *   "\n{hhhhhhhh} "
 *
 * In this template, the 'h' characters represent lower-case hexadecimal
 * characters that encode the CRC32 checksum of all the previous characters
 * (including newline characters, but excluding the actual checksum tag, i.e.
 * the curly brackets, the hexadecimal characters in between them and the
 * tailing space character).
 *
 * We assume the newline character is already present in the output prior to
 * calling this method.
 *
 * The checksumming starts only after the first checksum tag which is:
 *
 *   "\n{00000000} "
 */
IMPLEMENT inline
void Uart::checksum_tag()
{
  Unsigned32 val = crc32.get();
  char dump[11];

  dump[0] = '{';
  dump[9] = '}';
  dump[10] = ' ';

  for (unsigned int i = 8; i > 0; --i)
    {
      Unsigned8 nibble = val & 0x0f;

      // Encode nibble
      if (nibble < 10)
        dump[i] = '0' + nibble;
      else
        dump[i] = 'a' + (nibble - 10);

      val >>= 4;
    }

  uart()->write(dump, 11);
}

IMPLEMENT_OVERRIDE inline
int Uart::write(char const *str, __SIZE_TYPE__ count)
{
  if (crc32.empty())
    {
      // We are about to print the first characters. Thus indicate we initialize
      // the checksumming by printing the first checksum tag.
      //
      // This might actually produce an extra newline in the output stream which
      // we have inherited from the boot loader, but the newline character is
      // essential.
      uart()->write("\r\n", 2);
      checksum_tag();
    }

  char const *cur = str;
  char const *end = str + count;
  char const *delim = str;

  while (cur < end)
    {
      // Find the delimiter in the current part of the string which points
      // either to the next newline character or to the end of the string.
      while (delim < end && *delim != '\n')
        ++delim;

      // Output the current part up to (but not including) the delimiter.
      while (cur < delim)
        {
          int out = uart()->write(cur, delim - cur);
          if (out < 0)
            return out;

          crc32.checksum(cur, delim - cur);
          cur += out;
        }

      // If the current delimiter is a newline, then output LF followed by
      // a checksum tag and move to the next part of the string.
      if (delim < end && *delim == '\n')
        {
          int out = uart()->write("\n", 1);
          if (out < 0)
            return out;

          // Output the checksum tag. The newline character is a valid part of
          // the checksum, but it is also the start of the checksum tag.
          crc32.checksum("\n", 1);
          checksum_tag();
          ++delim;
          ++cur;
        }
    }

  return count;
}
