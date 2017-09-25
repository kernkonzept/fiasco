#ifndef PANIC_H
#define PANIC_H

#include <cdefs.h>
#include <fiasco_defs.h>

__BEGIN_DECLS

void FIASCO_COLD panic (const char *format, ...)
  __attribute__ ((__noreturn__))
  __attribute__ ((format(printf, 1, 2)));

__END_DECLS

#endif
