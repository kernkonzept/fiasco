#include <errno.h>
#ifndef LIBCL4
#include "pthread_impl.h"
#else /* LIBCL4 */
static int __single_threaded_errno;
#endif /* LIBCL4 */

int *__errno_location(void)
{
#ifndef LIBCL4
	return &__pthread_self()->errno_val;
#else /* LIBCL4 */
        return &__single_threaded_errno;
#endif /* LIBCL4 */
}

#ifndef LIBCL4
weak_alias(__errno_location, ___errno_location);
#endif /* LIBCL4 */
