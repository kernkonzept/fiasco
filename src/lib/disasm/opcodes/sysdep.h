#include <stdlib.h>
#include <string.h>
#include <panic.h>

#define abort()		panic("Error is i386-dis.c line %d", __LINE__)
