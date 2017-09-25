#ifndef _STDLIB_H
#define _STDLIB_H

#include <cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

int atexit(void (*function)(void));

long int strtol(const char *nptr, char **endptr, int base);
unsigned long int strtoul(const char *nptr, char **endptr, int base);

extern int __ltostr(char *s, unsigned int size, unsigned long i, unsigned int base, int UpCase);
extern int __dtostr(double d,char *buf,unsigned int maxlen,unsigned int prec);

#ifndef __STRICT_ANSI__
__extension__ long long int strtoll(const char *nptr, char **endptr, int base);
__extension__ unsigned long long int strtoull(const char *nptr, char **endptr, int base);
__extension__ int __lltostr(char *s, unsigned int size, unsigned long long i, unsigned int base, int UpCase);
#endif

int atoi(const char *nptr);
long int atol(const char *nptr);
double atof(const char *nptr);

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

void _exit(int status) __attribute__((noreturn));
void exit(int status) __attribute__((noreturn));
void abort(void);

/* warning: the rand() implementation of the diet libc really sucks. */
#define RAND_MAX 32767

typedef struct { int quot,rem; } div_t;
div_t div(int numer, int denom) __attribute__((const));

typedef struct { long int quot,rem; } ldiv_t;
ldiv_t ldiv(long int numer, long int denom) __attribute__((const));

typedef struct { long long int quot,rem; } lldiv_t;
lldiv_t lldiv(long long int numer, long long int denom) __attribute__((const));

int abs(int i) __attribute__((const));
long int labs(long int i) __attribute__((const));
__extension__ long long int llabs(long long int i) __attribute__((const));

__END_DECLS

#endif
