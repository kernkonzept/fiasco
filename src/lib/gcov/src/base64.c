/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020, 2023 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */
#include "base64.h"
#include "llvmcov.h"

#ifdef L4API_l4f
  #include <l4/sys/vcon.h> //For non-bootstrap version
#else
  #ifndef L4API_
    #include <stdio.h> //For the kernel
  #else
    #include <stddef.h> //NULL For bootstrap
  #endif              // L4API_
#endif              // L4API_l4f

GLOBAL_DATA char buffer[BUF_SIZE];
GLOBAL_DATA char outbuf[BUF_SIZE * 4 / 3];
_Static_assert(BUF_SIZE % 3 == 0, "BUF_SIZE is multiple of 3");


// The base64 encoding table
GLOBAL_DATA char const
  *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


// length of out must be enough to hold base64 string, e.g. for 10 chars -> 16!
long
print_base64(char const *s, int l, char *out)
{
  char *pos = out;

  if (!s)
    return 0;

  char const *end = s + l;
  while (end - s >= 3)
    {
      // As long as we have new data for four base64 bytes
      *pos++ = b64[(unsigned char)s[0] >> 2];
      *pos++ = b64[((unsigned char)s[0] & 0x3) << 4 | ((unsigned char)s[1] >> 4)];
      *pos++ = b64[((unsigned char)s[1] & 0xf) << 2 | ((unsigned char)s[2] >> 6)];
      *pos++ = b64[(unsigned char)s[2] & 0x3f];
      s += 3;
    }

  if (end - s)
    {
      *pos++ = b64[(unsigned char)s[0] >> 2];
      if (end - s == 1)
        {
          *pos++ = b64[((unsigned char)s[0] & 0x03) << 4];
          *pos++ = '=';
        }
      else
        {
          *pos++ = b64[((unsigned char)s[0] & 0x03) << 4 | (unsigned char)s[1] >> 4];
          *pos++ = b64[((unsigned char)s[1] & 0x0f) << 2];
        }
      *pos++ = '=';
    }
  //printf("printing out %d bytes. returning %d\n", l, pos - out);
  return pos - out;
}


void
store_u32(unsigned long v)
{
  store(&v, 4);
}
