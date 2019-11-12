INTERFACE:

#include "mux_console.h"
#include "std_macros.h"
#include "global_data.h"

class Kconsole : public Mux_console
{
public:
  /**
   * Get the pointer to \ref _c.
   *
   * \return Pointer to \ref _c.
   */
  static Mux_console *console() FIASCO_CONST
  {
    return _c;
  }

  /**
   * Get the pointer to \ref _c.
   *
   * This is a special variant of the \ref console() method that does not
   * check whether \ref _c has been constructed before. It is only supposed
   * to be called from the implementation of \ref assert_fail() to avoid
   * infinite recursion.
   *
   * \return Pointer to \ref _c.
   */
  static Mux_console *console_unchecked() FIASCO_CONST
  {
    return _c.get_unchecked();
  }

private:
  static Global_data<Static_object<Kconsole>> _c;
};

IMPLEMENTATION:

#include "config.h"
#include "console.h"
#include "mux_console.h"
#include "processor.h"

DEFINE_GLOBAL Global_data<Static_object<Kconsole>> Kconsole::_c;

PUBLIC inline
Kconsole::Kconsole()
{
  Console::stdout = this;
  Console::stderr = this;
  if constexpr (TAG_ENABLED(input))
    Console::stdin  = this;
}


PUBLIC static FIASCO_NOINLINE
void
Kconsole::init()
{ _c.construct(); }

// -------------------------------------------------------------------------
IMPLEMENTATION [input]:

PUBLIC
int Kconsole::getchar(bool blocking) override
{
  if (!blocking)
    return Mux_console::getchar(false);

  while (1)
    {
      int c;
      if ((c = Mux_console::getchar(false)) != -1)
        return c;

      Proc::pause();
    }
}
