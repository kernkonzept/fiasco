INTERFACE:

#include "console.h"

class Push_console : public Console
{
public:
  Push_console() : Console(ENABLED) {}
private:
  char _buffer[256];
  int _in;
  int _out;
};

IMPLEMENTATION:

#include "keycodes.h"

PUBLIC
int
Push_console::getchar(bool /*blocking*/)
{
  if (_out != _in)
    {
      int c = _buffer[_out++];
      if (_out >= (int)sizeof(_buffer))
	_out = 0;

      return c == '_' ? KEY_RETURN : c;
    }

  return -1; // no keystroke available
}

PUBLIC
int
Push_console::char_avail() const
{
  return _in != _out; // unknown
}

PUBLIC
int
Push_console::write(char const * /*str*/, size_t len)
{
  return len;
}

PUBLIC
void
Push_console::push(char c)
{
  int _ni = _in + 1;
  if (_ni >= (int)sizeof(_buffer))
    _ni = 0;

  if (_ni == _out) // buffer full
    return;

  _buffer[_in] = c;
  _in = _ni;
}

PUBLIC inline
void
Push_console::flush()
{
  _in = _out = 0;
}

PUBLIC inline
Mword
Push_console::get_attributes() const
{
  return PUSH | IN;
}
