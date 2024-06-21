#pragma once

#define FIASCO_COLD __attribute__((cold))
#define FIASCO_PURE __attribute__((pure))

#define FIASCO_MACRO_JOIN(a, b) a ## b

#define FIASCO_U_CONST(value)   FIASCO_MACRO_JOIN(value, U)
#define FIASCO_UL_CONST(value)  FIASCO_MACRO_JOIN(value, UL)
#define FIASCO_ULL_CONST(value) FIASCO_MACRO_JOIN(value, ULL)

#ifdef __has_builtin
#define FIASCO_HAS_BUILTIN(def) __has_builtin(def)
#else
#define FIASCO_HAS_BUILTIN(def) 0
#endif
