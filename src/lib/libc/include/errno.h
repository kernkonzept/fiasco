#ifndef	_ERRNO_H
#define _ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#include <bits/errno.h>

#ifndef LIBCL4

#ifdef __GNUC__
__attribute__((const))
#endif
int *__errno_location(void);
#define errno (*__errno_location())

#ifdef _GNU_SOURCE
extern char *program_invocation_short_name, *program_invocation_name;
#endif

#endif /* LIBCL4 */

#ifdef __cplusplus
}
#endif

#endif

