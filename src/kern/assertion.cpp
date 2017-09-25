IMPLEMENTATION:

#include <cassert>
#include <cstdio>
#include <stdlib.h>

// ------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "kernel_console.h"
#include "thread.h"

extern "C"
void
assert_fail(char const *expr_msg, char const *file, unsigned int line)
{
  // make sure that GZIP mode is off
  Kconsole::console()->end_exclusive(Console::GZIP);

  printf("\nAssertion failed at %s:%u: %s\n", file, line, expr_msg);

  Thread::system_abort();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [!debug]:

#include "terminate.h"

extern "C"
void
assert_fail(char const *expr_msg, char const *file, unsigned int line)
{
  printf("\nAssertion failed at %s:%u: %s\n", file, line, expr_msg);

  terminate(EXIT_FAILURE);
}
