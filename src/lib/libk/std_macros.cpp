INTERFACE:

#define BUILTIN_EXPECT(exp,c)	__builtin_expect((exp),(c))
#define EXPECT_TRUE(exp)	__builtin_expect((exp),true)
#define EXPECT_FALSE(exp)	__builtin_expect((exp),false)

// Use this for functions which do not examine any values except their
// arguments and have no effects except the return value. Note that a
// function that has pointer arguments and examines the data pointed to
// must _not_ be declared `const'.  Likewise, a function that calls a
// non-`const' function usually must not be `const'.  It does not make
// sense for a `const' function to return `void'.
#define FIASCO_CONST		__attribute__ ((__const__))

#ifdef __i386__
#define FIASCO_FASTCALL		__attribute__ ((__regparm__(3)))
#else
#define FIASCO_FASTCALL
#endif

#define MARK_AS_DEPRECATED	__attribute__ ((__deprecated__))
#define ALWAYS_INLINE		__attribute__ ((__always_inline__))
#define FIASCO_NOINLINE        __attribute__ ((__noinline__))
#define FIASCO_WARN_RESULT     __attribute__ ((warn_unused_result))

#define FIASCO_NORETURN         __attribute__ ((__noreturn__))
#define FIASCO_FLATTEN          __attribute__((__flatten__))

#define FIASCO_STRINGIFY_(x) #x
#define FIASCO_STRINGIFY(x) FIASCO_STRINGIFY_(x)

IMPLEMENTATION:
//-
