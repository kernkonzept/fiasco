INTERFACE:

#include <cstdarg>
#include <cstdio>

void register_libc_atexit(void (*f)(void));


IMPLEMENTATION:

#include <cstdlib>
#include <unistd.h>
#include "kernel_console.h"
#include "panic.h"


static void (*libc_atexit)(void);


void
register_libc_atexit(void (*f)(void))
{
  if (libc_atexit)
    panic("libc_atexit already registered");
  libc_atexit = f;
}

// Our own version of the assertion failure output function according
// to Linux Standard Base Specification.
// We need it since the standard glibc function calls printf, and doing
// that on a Fiasco kernel stack blows everything up.
extern "C"
void
__assert_fail (const char *assertion, const char *file,
               unsigned int line, const char *function)
{
  Kconsole::console()->change_state(0, 0, ~0U, Console::OUTENABLED);

  printf ("ASSERTION_FAILED (%s)\n"
          "  in function %s\n"
	  "  in file %s:%d\n",
          assertion, function, file, line);

  if (libc_atexit)
    libc_atexit();
  _exit (EXIT_FAILURE);         // Fatal! No destructors
}
