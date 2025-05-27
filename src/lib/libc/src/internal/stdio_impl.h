#ifndef _STDIO_IMPL_H
#define _STDIO_IMPL_H

#include <stdio.h>
#include "syscall.h"
#include "libc_backend.h"

#define UNGET 8

#define FLOCK(f) unsigned long lock_state = __libc_backend_printf_lock();
#define FUNLOCK(f) __libc_backend_printf_unlock(lock_state)

struct _IO_FILE {
	unsigned char *rpos;
	unsigned char *wend, *wpos;
	unsigned char *wbase;
	size_t (*write)(FILE *, const unsigned char *, size_t);
	unsigned char *buf;
	size_t buf_size;
	void *cookie;
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

#define ferror(f) 0

/* Caller-allocated FILE * operations */
hidden FILE *__fopen_rb_ca(const char *, FILE *, unsigned char *, size_t);
hidden int __fclose_ca(FILE *);

#endif
