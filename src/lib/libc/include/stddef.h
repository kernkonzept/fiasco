#ifndef _STDDEF_H
#define _STDDEF_H

#include <features.h>

#if defined(__cplusplus)
#define NULL nullptr
#else
#define NULL ((void*)0)
#endif

#define __NEED_ptrdiff_t
#define __NEED_size_t
#define __NEED_wchar_t
#define __NEED_max_align_t

#include <bits/alltypes.h>

#define offsetof(type, member) __builtin_offsetof(type, member)

#endif
