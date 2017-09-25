/* This file defines the back-end interface */
/* of the kernel C-library. */

#ifndef __LIBC_BACKEND_H__
#define __LIBC_BACKEND_H__

#include <stddef.h>
#include <cdefs.h>

__BEGIN_DECLS

/**
 * The text output back-end.
 *
 * This function must be provided to the C-library for
 * text output. It must simply send len characters of s
 * to an output device.
 *
 * @param s   Buffer to send.
 * @param len The number of bytes.
 * @return 1 on success, 0 else.
 */
int __libc_backend_outs(const char *s, size_t len);

/**
 * The text input back-end.
 *
 * This function must be provided to the C-library for
 * text input. It has to block until len characters are
 * read or a newline is reached. The return value gives
 * the number of characters virtually read.
 *
 * @param s   A pointer to the buffer for the read text.
 * @param len The size of the buffer.
 * @return the number of characters virtually read.
 */
int __libc_backend_ins(char *s, size_t len);

typedef unsigned long __libc_backend_printf_lock_t;

__libc_backend_printf_lock_t __libc_backend_printf_lock(void);

void __libc_backend_printf_unlock(__libc_backend_printf_lock_t);

void __libc_backend_printf_local_force_unlock();

__END_DECLS

#endif //__LIBC_BACKEND_H__
