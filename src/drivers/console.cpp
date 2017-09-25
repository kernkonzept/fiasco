INTERFACE:

#include <cstddef>
#include "l4_types.h"

/**
 * The abstract interface for a text I/O console.
 *
 * This abstract interface can be implemented for virtually every
 * text input or output device.
 */
class Console
{
public:
  enum Console_state
  {
    DISABLED    =     0,
    INENABLED   =     1, ///< output channel of console enabled
    OUTENABLED  =     2, ///< input channel of console enabled
    ENABLED     =     INENABLED | OUTENABLED ///< console fully enabled
  };

  enum Console_attr
  {
    // universal attributes
    INVALID     =     0,
    OUT         =   0x1, ///< output to console is possible
    IN          =   0x2, ///< input from console is possible
    // attributes to identify a specific console
    DIRECT      =   0x4, ///< output to screen or input from keyboard
    UART        =   0x8, ///< output to/input from serial serial line
    UX          =  0x10, ///< filtered input console for UX
    PUSH        =  0x20, ///< input console
    GZIP        =  0x40, ///< gzip+uuencode output and sent to uart console
    BUFFER      =  0x80, ///< ring buffer
    DEBUG       = 0x100, ///< kdb interface
    FAILED      = 0x200, ///< initialization failed
  };

  /**
   * modify console state
   */
  virtual void state(Mword new_state);

  /**
   * Write a string of len chacters to the output.
   * @param str the string to write (no zero termination is needed)
   * @param len the number of chacters to write.
   *
   * This method must be implemented in every implementation, but
   * can simply do nothing for input only consoles.
   */
  virtual int write(char const *str, size_t len);

  /**
   * read a charcater from the input.
   * @param blocking if true getchar blocks til a charcater is available.
   *
   * This method must be implemented in every implementation, but
   * can simply return -1 for output only consoles.
   */
  virtual int getchar(bool blocking = true);

  /**
   * Is input available?
   *
   * This method can be implemented.
   * It must return -1 if no information is available, 
   * 1 if at least one character is avialable, and 0 if
   * no charachter is available.
   */
  virtual int char_avail() const;

  /**
   * Console attributes.
   */
  virtual Mword get_attributes() const;

  virtual ~Console();

  explicit Console(Console_state state) : _state(state) {}

  void add_state(Console_state state)
  { _state |= state; }

  void del_state(Console_state state)
  { _state &= ~state; }

public:
  /**
   * Disables the stdout, stdin, and stderr console.
   */
  static void disable_all();

  /// stdout for libc glue.
  static Console *stdout;
  /// stderr for libc glue.
  static Console *stderr;
  /// stdin for libc glue.
  static Console *stdin;

protected:
  Mword  _state;
};



IMPLEMENTATION:

#include <cstring>
#include <cctype>

Console *Console::stdout;
Console *Console::stderr;
Console *Console::stdin;

IMPLEMENT Console::~Console()
{}

/**
 * get current console state
 */
PUBLIC inline
Mword
Console::state() const
{
  return _state;
}

IMPLEMENT
void Console::state(Mword new_state)
{
  _state = new_state;
}

PUBLIC inline
bool
Console::failed() const
{
  return _state & FAILED;
}

PUBLIC inline
void
Console::fail()
{
  _state |= FAILED;
}

IMPLEMENT
void Console::disable_all()
{
  stdout = 0;
  stderr = 0;
  stdin  = 0;
}

IMPLEMENT
int Console::write(char const *, size_t len)
{
  return len;
}

IMPLEMENT
int Console::getchar(bool /* blocking */)
{
  return -1; /* no input */
}

IMPLEMENT
int Console::char_avail() const
{
  return -1; /* unknown */
}

IMPLEMENT
Mword Console::get_attributes() const
{
  return 0;
}

IMPLEMENTATION[debug]:

PUBLIC
const char*
Console::str_mode() const
{
  static char const * const mode_str[] =
    { "      ", "Output", "Input ", "InOut " };
  return mode_str[get_attributes() & (OUT|IN)];
}

PUBLIC
const char*
Console::str_state() const
{
  static char const * const state_str[] =
    { "Disabled       ", "Output disabled",
      "Input disabled ", "Enabled        " };
  return state_str[state() & ENABLED];
}

PUBLIC
const char*
Console::str_attr(Mword bit) const
{
  static char const * const attr_str[] =
    { "Direct", "Uart", "UX", "Push", "Gzip", "Buffer", "Kdb", "FAILED!" };

  return (bit < 2 || bit >= (sizeof(attr_str)/sizeof(attr_str[0]))+2)
    ? "???"
    : attr_str[bit-2];
}

