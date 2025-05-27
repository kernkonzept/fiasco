#include <errno.h>

static int __single_threaded_errno;

int *__errno_location(void)
{
        return &__single_threaded_errno;
}
