#pragma once

#include <assert.h>
#include <fiasco_defs.h>

#if defined(NDEBUG)
# define assert_opt(expr) do { if (!(expr)) __builtin_unreachable(); } while (0)
#else
# define assert_opt(expr) assert(expr)
#endif
