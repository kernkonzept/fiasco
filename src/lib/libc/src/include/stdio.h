#ifndef STDIO_H
#define STDIO_H

#define __DEFINED_struct__IO_FILE

#include "../../include/stdio.h"

#ifndef LIBCL4
#undef stdin
#undef stdout
#undef stderr

extern hidden FILE __stdin_FILE;
extern hidden FILE __stdout_FILE;
extern hidden FILE __stderr_FILE;

#define stdin (&__stdin_FILE)
#define stdout (&__stdout_FILE)
#define stderr (&__stderr_FILE)
#else /* LIBCL4 */
extern hidden FILE __stdout_FILE;
#define libc_stdout (&__stdout_FILE)
#endif /* LIBCL4 */

#endif
