INTERFACE:

#include <cstdio>

#include "config.h"

enum Warn_level
{
  Error   = 0,
  Warning = 1,
  Info    = 2,
};

#define WARNX(level,fmt...) \
  do {							\
       if (level   <= (int)Config::Warn_level)		\
	 printf("\n\033[31mKERNEL\033[m: Warning: " fmt);	\
     } while (0)

#define WARN(fmt...) WARNX(Warning, fmt)

