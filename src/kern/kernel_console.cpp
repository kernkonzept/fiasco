INTERFACE:

#include "mux_console.h"
#include "std_macros.h"

class Kconsole : public Mux_console
{
public:
  int  getchar(bool blocking = true);
  void getchar_chance();

  static Mux_console *console() FIASCO_CONST
  { return _c; }

private:
  static Static_object<Kconsole> _c;
};

IMPLEMENTATION:

#include "config.h"
#include "console.h"
#include "mux_console.h"
#include "processor.h"

Static_object<Kconsole> Kconsole::_c;


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
          && Proc::interrupts())            // does'nt work without ints
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


