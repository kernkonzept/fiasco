#ifndef _STDIO_H
#define _STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "globalconfig.h"

#include <features.h>

#define __NEED_FILE
#define __NEED___isoc_va_list
#define __NEED_size_t

#if defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) \
 || defined(_XOPEN_SOURCE) || defined(_GNU_SOURCE) \
 || defined(_BSD_SOURCE)
#define __NEED_ssize_t
#define __NEED_off_t
#define __NEED_va_list
#endif

#include <bits/alltypes.h>

#if defined(__cplusplus) && __cplusplus >= 201103L
#define NULL nullptr
#elif defined(__cplusplus)
#define NULL 0L
#else
#define NULL ((void*)0)
#endif

#undef EOF
#define EOF (-1)

#ifdef CONFIG_OUTPUT
int getchar(void);

int putchar(int);
int puts(const char *);
int printf(const char *__restrict, ...);
int sprintf(char *__restrict, const char *__restrict, ...);
int snprintf(char *__restrict, size_t, const char *__restrict, ...);

int vprintf(const char *__restrict, __isoc_va_list);
int vsprintf(char *__restrict, const char *__restrict, __isoc_va_list);
int vsnprintf(char *__restrict, size_t, const char *__restrict, __isoc_va_list);
#else
static inline int getchar(void)
{ return EOF; }

static inline int putchar(int c)
{ return c; }

static inline int puts(const char *)
{ return 0; }

static inline int printf(const char *__restrict, ...)
{ return 0; }
static inline int sprintf(char *__restrict, const char *__restrict, ...)
{ return 0; }
static inline int snprintf(char *__restrict, size_t, const char *__restrict, ...)
{ return 0; }
static inline int vprintf(const char *__restrict, __isoc_va_list)
{ return 0; }
static inline int vsprintf(char *__restrict, const char *__restrict, __isoc_va_list)
{ return 0; }
static inline int vsnprintf(char *__restrict, size_t, const char *__restrict, __isoc_va_list)
{ return 0; }
#endif

#ifdef __cplusplus
}
#endif

#endif
