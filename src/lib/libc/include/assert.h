#pragma once

#include <features.h>
#include <cdefs.h>

__BEGIN_DECLS

void
assert_fail(char const *expr_msg, char const *file, unsigned int line,
            void const *caller)
     __attribute__ ((__noreturn__));

__END_DECLS

#define ASSERT_EXPECT_FALSE(exp)  __builtin_expect((exp), 0)

#undef assert
#ifdef NDEBUG
#define assert(expr) do {} while (0)
#define check(expr) (void)(expr)
#else
# define assert(expr)						\
  ((void)((ASSERT_EXPECT_FALSE(!(expr)))			\
	  ? (assert_fail(#expr, __FILE__, __LINE__,             \
                         __builtin_return_address(0)), 0)	\
	  : 0))
# define check(expr) assert(expr)
#endif
