#ifndef __FIASCO_FORMAT_H
#define __FIASCO_FORMAT_H

#ifndef ASSEMBLER

/* This declaration is needed since printf is not checked per default
 * when using with C++ */

#include <sys/cdefs.h>

__BEGIN_DECLS

int printf(const char *format, ...) __attribute__((format (printf,1,2) ));

__END_DECLS

#endif

#endif

