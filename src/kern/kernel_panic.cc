#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <panic.h>

#include "kernel_console.h"
#include "simpleio.h"
#include "thread.h"

void
panic(const char *format, ...)
{
  // make sure that GZIP mode is off
  Kconsole::console()->end_exclusive(Console::GZIP);

  va_list args;

  putstr("\033[1mPanic: ");
  va_start (args, format);
  vprintf  (format, args);
  va_end   (args);
  putstr("\033[m");

  Thread::system_abort();
}
