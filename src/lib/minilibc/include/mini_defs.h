#ifndef MINIDEFS_INCLUDE
#define MINIDEFS_INCLUDE

#if(__GNUC__ >= 3)
#  define MARK_DEPRECATED __attribute__((deprecated))
#else
#  define MARK_DEPRECATED /*empty*/
#endif



#endif // MINIDEFS_INCLUDE
