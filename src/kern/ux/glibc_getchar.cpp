IMPLEMENTATION:

#include <cstdio>
#include "console.h"
#include "filter_console.h"
#include "glibc_getchar.h"
#include "kernel_console.h"
#include "static_init.h"

class Glibc_getchar : public Console
{
public:
  Glibc_getchar() : Console(ENABLED) {}
};

PUBLIC
int
Glibc_getchar::getchar(bool blocking)
{
  (void)blocking;
  return ::getchar();
}

PUBLIC
int
Glibc_getchar::write(char const *str, size_t len)
{
  (void)str; (void)len;
  return 1;
}

PUBLIC
Mword
Glibc_getchar::get_attributes() const
{
  return UX | IN;
}

static void
glibc_flt_getchar_init()
{
  static Glibc_getchar c;
  static Filter_console fcon(&c, 0);

  Kconsole::console()->register_console(&fcon);
}

STATIC_INITIALIZER_P(glibc_flt_getchar_init, GDB_INIT_PRIO);
