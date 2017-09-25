#ifndef	_CTYPE_H
#define _CTYPE_H

#include <cdefs.h>

__BEGIN_DECLS

int isascii (int c) __attribute__ ((__const__));
int isblank (int c) __attribute__ ((__const__));
int isalnum (int c) __attribute__ ((__const__));
int isalpha (int c) __attribute__ ((__const__));
int isdigit (int c) __attribute__ ((__const__));
int isspace (int c) __attribute__ ((__const__));

int isupper (int c) __attribute__ ((__const__));
int islower (int c) __attribute__ ((__const__));

int tolower(int c) __attribute__ ((__const__));
int toupper(int c) __attribute__ ((__const__));

int isprint(int c) __attribute__ ((__const__));
int ispunct(int c) __attribute__ ((__const__));
int iscntrl(int c) __attribute__ ((__const__));

/* fscking GNU extensions! */
int isxdigit(int c) __attribute__ ((__const__));

int isgraph(int c) __attribute__ ((__const__));

__END_DECLS

#endif
