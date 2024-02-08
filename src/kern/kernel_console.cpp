INTERFACE:

#include "mux_console.h"
#include "std_macros.h"
#include "global_data.h"

class Kconsole : public Mux_console
{
public:
  int  getchar(bool blocking = true) override;
  void getchar_chance();

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


IMPLEMENT
int Kconsole::getchar(bool blocking)
{
  if (!blocking)
    return Mux_console::getchar(false);

  while (1)
    {
      int c;
      if ((c = Mux_console::getchar(false)) != -1)
        return c;

      if (Config::getchar_does_hlt_works_ok // wakeup timer is enabled
          && Proc::interrupts())            // doesn't work without ints
        Proc::halt();
      else
        Proc::pause();
    }
}


PUBLIC inline
Kconsole::Kconsole()
{
  Console::stdout = this;
  Console::stderr = this;
  Console::stdin  = this;
}


PUBLIC static FIASCO_NOINLINE
void
Kconsole::init()
{ _c.construct(); }


