INTERFACE:

#include "initcalls.h"

class Banner
{
};


IMPLEMENTATION:

#include <cstdio>
#include "config.h"

PUBLIC static FIASCO_INIT
void
Banner::init()
{
  extern char _initkip_start[];
  printf("\n%s\n", _initkip_start);
  if (Config::kernel_warn_config_string && *Config::kernel_warn_config_string)
    printf("\033[31mPerformance-critical config option(s) detected:\n"
	   "%s\033[m", Config::kernel_warn_config_string);
  putchar('\n');
}
