#define _Addr long
#define _Int64 long long
#define _Reg long

#define __BYTE_ORDER 1234
#if __SIZEOF_LONG__ == 8
#define __LONG_MAX 0x7fffffffffffffffL
#else
#define __LONG_MAX 0x7fffffffL
#endif

#ifndef __cplusplus
#ifdef __WCHAR_TYPE__
#if defined(__NEED_wchar_t) && !defined(__DEFINED_wchar_t)
typedef __WCHAR_TYPE__ wchar_t;
#define __DEFINED_wchar_t
#endif

#else
#if defined(__NEED_wchar_t) && !defined(__DEFINED_wchar_t)
typedef long wchar_t;
#define __DEFINED_wchar_t
#endif

#endif
#endif

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __USE_TIME_BITS64 1

#if defined(__NEED_size_t) && !defined(__DEFINED_size_t)
#if 0
typedef unsigned _Addr size_t;
#else
typedef __SIZE_TYPE__ size_t;
#endif
#define __DEFINED_size_t
#endif

#if defined(__NEED_uintptr_t) && !defined(__DEFINED_uintptr_t)
typedef unsigned _Addr uintptr_t;
#define __DEFINED_uintptr_t
#endif

#if defined(__NEED_ptrdiff_t) && !defined(__DEFINED_ptrdiff_t)
typedef _Addr ptrdiff_t;
#define __DEFINED_ptrdiff_t
#endif

#if defined(__NEED_ssize_t) && !defined(__DEFINED_ssize_t)
typedef _Addr ssize_t;
#define __DEFINED_ssize_t
#endif

#if defined(__NEED_intptr_t) && !defined(__DEFINED_intptr_t)
typedef _Addr intptr_t;
#define __DEFINED_intptr_t
#endif


#if defined(__NEED_int8_t) && !defined(__DEFINED_int8_t)
typedef signed char     int8_t;
#define __DEFINED_int8_t
#endif

#if defined(__NEED_int16_t) && !defined(__DEFINED_int16_t)
typedef signed short    int16_t;
#define __DEFINED_int16_t
#endif

#if defined(__NEED_int32_t) && !defined(__DEFINED_int32_t)
typedef signed int      int32_t;
#define __DEFINED_int32_t
#endif

#if defined(__NEED_int64_t) && !defined(__DEFINED_int64_t)
typedef signed _Int64   int64_t;
#define __DEFINED_int64_t
#endif

#if defined(__NEED_intmax_t) && !defined(__DEFINED_intmax_t)
typedef signed _Int64   intmax_t;
#define __DEFINED_intmax_t
#endif

#if defined(__NEED_uint8_t) && !defined(__DEFINED_uint8_t)
typedef unsigned char   uint8_t;
#define __DEFINED_uint8_t
#endif

#if defined(__NEED_uint16_t) && !defined(__DEFINED_uint16_t)
typedef unsigned short  uint16_t;
#define __DEFINED_uint16_t
#endif

#if defined(__NEED_uint32_t) && !defined(__DEFINED_uint32_t)
typedef unsigned int    uint32_t;
#define __DEFINED_uint32_t
#endif

#if defined(__NEED_uint64_t) && !defined(__DEFINED_uint64_t)
typedef unsigned _Int64 uint64_t;
#define __DEFINED_uint64_t
#endif

#if defined(__NEED_u_int64_t) && !defined(__DEFINED_u_int64_t)
typedef unsigned _Int64 u_int64_t;
#define __DEFINED_u_int64_t
#endif

#if defined(__NEED_uintmax_t) && !defined(__DEFINED_uintmax_t)
typedef unsigned _Int64 uintmax_t;
#define __DEFINED_uintmax_t
#endif


#if defined(__NEED_off_t) && !defined(__DEFINED_off_t)
typedef _Int64 off_t;
#define __DEFINED_off_t
#endif


#if defined(__NEED_struct__IO_FILE) && !defined(__DEFINED_struct__IO_FILE)
struct _IO_FILE { char __x; };
#define __DEFINED_struct__IO_FILE
#endif

#if defined(__NEED_FILE) && !defined(__DEFINED_FILE)
typedef struct _IO_FILE FILE;
#define __DEFINED_FILE
#endif


#if defined(__NEED_va_list) && !defined(__DEFINED_va_list)
typedef __builtin_va_list va_list;
#define __DEFINED_va_list
#endif

#if defined(__NEED___isoc_va_list) && !defined(__DEFINED___isoc_va_list)
typedef __builtin_va_list __isoc_va_list;
#define __DEFINED___isoc_va_list
#endif


#undef _Addr
#undef _Int64
#undef _Reg
