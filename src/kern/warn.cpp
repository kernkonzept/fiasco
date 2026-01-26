INTERFACE:

#include <cstdio>

#include "config.h"

enum Warn_level
{
  Error   = 0,
  Warning = 1,
  Info    = 2,
};

namespace Warn
{

constexpr bool is_enabled(Warn_level level)
{ return level <= static_cast<int>(Config::Warn_level); }

}

#define WARNX(level, ...) \
  do {                                                            \
       if constexpr (Warn::is_enabled(level))                     \
         printf("\n\033[31mKERNEL\033[m: " #level ": " __VA_ARGS__); \
     } while (0)

#define WARN(...) WARNX(Warning, __VA_ARGS__)

