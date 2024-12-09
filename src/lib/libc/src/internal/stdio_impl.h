#ifndef _STDIO_IMPL_H
#define _STDIO_IMPL_H

#include <stdio.h>
#include "syscall.h"
#ifdef LIBCL4
#include "libc_backend.h"
#endif /* LIBCL4 */

#define UNGET 8

#define FFINALLOCK(f) ((f)->lock>=0 ? __lockfile((f)) : 0)
#ifndef LIBCL4
#define FLOCK(f) int __need_unlock = ((f)->lock>=0 ? __lockfile((f)) : 0)
#define FUNLOCK(f) do { if (__need_unlock) __unlockfile((f)); } while (0)
#else /* LIBCL4 */
#define FLOCK(f) unsigned long lock_state = __libc_backend_printf_lock();
#define FUNLOCK(f) __libc_backend_printf_unlock(lock_state)
#endif /* LIBCL4 */

#ifndef LIBCL4
#define F_PERM 1
#define F_NORD 4
#define F_NOWR 8
#define F_EOF 16
#define F_ERR 32
#define F_SVB 64
#define F_APP 128
#endif

/* LIBCL4: save some stack. Maybe remove even more. */
struct _IO_FILE {
#ifndef LIBCL4
	unsigned flags;
#endif
	unsigned char *rpos;
#ifndef LIBCL4
	int (*close)(FILE *);
#endif
	unsigned char *wend, *wpos;
#ifndef LIBCL4
	unsigned char *mustbezero_1;
#endif
	unsigned char *wbase;
	size_t (*read)(FILE *, unsigned char *, size_t);
	size_t (*write)(FILE *, const unsigned char *, size_t);
#ifndef LIBCL4
	off_t (*seek)(FILE *, off_t, int);
#endif
	unsigned char *buf;
	size_t buf_size;
#ifndef LIBCL4
	FILE *prev, *next;
	int fd;
	int pipe_pid;
	long lockcount;
	int mode;
#endif
	volatile int lock;
	int lbf;
	void *cookie;
#ifndef LIBCL4
	off_t off;
	char *getln_buf;
	void *mustbezero_2;
	unsigned char *shend;
	off_t shlim, shcnt;
	FILE *prev_locked, *next_locked;
#endif
};

extern hidden FILE *volatile __stdin_used;
extern hidden FILE *volatile __stdout_used;
extern hidden FILE *volatile __stderr_used;

hidden int __lockfile(FILE *);
hidden void __unlockfile(FILE *);

hidden size_t __stdio_read(FILE *, unsigned char *, size_t);
hidden size_t __stdio_write(FILE *, const unsigned char *, size_t);
hidden size_t __stdout_write(FILE *, const unsigned char *, size_t);
hidden off_t __stdio_seek(FILE *, off_t, int);
hidden int __stdio_close(FILE *);

hidden int __towrite(FILE *);

hidden void __stdio_exit(void);
hidden void __stdio_exit_needed(void);

hidden int __fseeko(FILE *, off_t, int);
hidden int __fseeko_unlocked(FILE *, off_t, int);
hidden off_t __ftello(FILE *);
hidden off_t __ftello_unlocked(FILE *);
hidden size_t __fwritex(const unsigned char *, size_t, FILE *);
hidden int __putc_unlocked(int, FILE *);

hidden FILE *__fdopen(int, const char *);
hidden int __fmodeflags(const char *);

hidden FILE *__ofl_add(FILE *f);
hidden FILE **__ofl_lock(void);
hidden void __ofl_unlock(void);

struct __pthread;
hidden void __register_locked_file(FILE *, struct __pthread *);
hidden void __unlist_locked_file(FILE *);
hidden void __do_orphaned_stdio_locks(void);

#define MAYBE_WAITERS 0x40000000

hidden void __getopt_msg(const char *, const char *, const char *, size_t);

#ifndef LIBCL4
#define feof(f) ((f)->flags & F_EOF)
#define ferror(f) ((f)->flags & F_ERR)
#else
#define ferror(f) 0
#endif

/* Caller-allocated FILE * operations */
hidden FILE *__fopen_rb_ca(const char *, FILE *, unsigned char *, size_t);
hidden int __fclose_ca(FILE *);

#endif
