/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020, 2023 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */
#include "output.h"

#ifdef L4API_l4f
  #include <l4/sys/vcon.h> //For non-bootstrap version
#else
  #ifndef L4API_
    #include <stdio.h> //For the kernel
  #else
    #include <stddef.h> //NULL For bootstrap
  #endif              // L4API_
#endif              // L4API_l4f

extern int
printf(char const *format, ...) __attribute__((weak));

void
vconprintn(char const *s, int len)
{
  if (printf)
    {
      printf("%.*s", len, s);
    }
  else
    {
#ifdef L4API_l4f
      char const *err = "Failed to write properly\n";
      long pos = 0;
      do
        {
          long written = l4_vcon_write(L4_BASE_LOG_CAP, s + pos, len - pos);
          if (written < 0)
            {
              l4_vcon_write(L4_BASE_LOG_CAP, err, __builtin_strlen(err));
              return;
            }
          pos += written;
        }
      while (len < pos);
#endif
    }
}

void
vconprint(char const *s)
{
  vconprintn(s, __builtin_strlen(s));
}



