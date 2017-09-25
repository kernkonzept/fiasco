#ifndef _LIMITS_H
#define _LIMITS_H

//#include <endian.h>

#define CHAR_BIT 8

#define SCHAR_MIN (-128)
#define SCHAR_MAX 0x7f
#define UCHAR_MAX 0xff

#define SHRT_MIN (-SHRT_MAX-1)
#define SHRT_MAX 0x7fff
#define USHRT_MAX 0xffff

#define INT_MIN (-INT_MAX-1)
#define INT_MAX 0x7fffffff
#define UINT_MAX 0xffffffff

#if __SIZEOF_POINTER__ == 8
#define LONG_MAX 9223372036854775807L
#define ULONG_MAX 18446744073709551615UL
#else
#define LONG_MAX 2147483647L
#define ULONG_MAX 4294967295UL
#endif
#define LONG_MIN (-LONG_MAX - 1L)

#define LLONG_MAX 9223372036854775807LL
#define LLONG_MIN (-LLONG_MAX - 1LL)

/* Maximum value an `unsigned long long int' can hold.  (Minimum is 0.)  */
#define ULLONG_MAX 18446744073709551615ULL
#define ULONG_LONG_MAX ULLONG_MAX

#define SSIZE_MIN INT_MIN
#define SSIZE_MAX INT_MAX

#define PASS_MAX	256

#define NR_OPEN		1024

#define NGROUPS_MAX	32	/* supplemental group IDs are available */
#define ARG_MAX		131072	/* # bytes of args + environ for exec() */
#define CHILD_MAX	999    /* no limit :-) */
#define OPEN_MAX	256	/* # open files a process may have */
#define LINK_MAX	127	/* # links a file may have */
#define MAX_CANON	255	/* size of the canonical input queue */
#define MAX_INPUT	255	/* size of the type-ahead buffer */
#define NAME_MAX	255	/* # chars in a file name */
#define PATH_MAX	4095	/* # chars in a path name */
#define PIPE_BUF	4096	/* # bytes in atomic write to a pipe */

#define RTSIG_MAX	32

#define LINE_MAX	2048

/* mutt demanded these */
#define _POSIX_PATH_MAX PATH_MAX
#define MB_LEN_MAX 16

#endif
