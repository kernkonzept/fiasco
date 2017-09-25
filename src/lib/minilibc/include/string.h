#ifndef _STRING_H
#define _STRING_H

#include <cdefs.h>
#include <stddef.h>

#include <memcpy.h>

__BEGIN_DECLS

char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);

void *memccpy(void *dest, const void *src, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);

int memccmp(const void *s1, const void *s2, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);

#if (__GNUC__>=3)
size_t strlen(const char *s) __attribute__((pure));
#else
size_t strlen(const char *s);
#endif

char *strstr(const char *haystack, const char *needle);

char *strdup(const char *s);

char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);

char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);

size_t strspn(const char *s, const char *_accept);
size_t strcspn(const char *s, const char *reject);

char *strpbrk(const char *s, const char *_accept);
char *strsep(char **stringp, const char *delim);

void* memset(void *s, int c, size_t n);
void* memchr(const void *s, int c, size_t n);

int strcoll(const char *s1, const char *s2);


__END_DECLS


#endif
