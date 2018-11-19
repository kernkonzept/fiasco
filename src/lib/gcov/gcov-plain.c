/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2019-2022 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */

#include "data-be.h"
#include "output-be.h"
#include <stddef.h>

// 192 is nice because it converts to 256 base64
enum
{
  BUF_SIZE = 192
};

static char buffer[BUF_SIZE];
static char outbuf[BUF_SIZE * 4 / 3];
static int buf_idx = 0;

// run length encoding (rle) mechanics using a counter buffer (cb)
static struct
{
  char init; // is initialized
  char last; // character
  int count; // how often the character was seen
} cb = {0, 0, 0};

// The base64 encoding table
extern const char b64[];

// Flush and print the runlength encoding buffer
static void
rle_flush(void)
{
  if (cb.init)
    {
      if (cb.count > 3)
        { // worth it to compress
          const char res[3] = {'@', cb.last, b64[cb.count]};
          vconprintn(res, 3);
        }
      else
        {
          char res[3];
          for (int c = 0; c < cb.count; c++)
            res[c] = cb.last;
          vconprintn(res, cb.count);
        }
    }
  cb.init = 0;
}

// add character to output
static void
rle(char const c)
{
  if (cb.init && cb.last == c && cb.count < 63)
    {
      // if the buffer is initialized, and the character is a repeat, and it was
      // repeated less than 63 times increase the count.
      cb.count++;
    }
  else
    {
      // otherwise flush buffer if filled and initialize with new character
      if (cb.init)
        rle_flush();
      cb.init  = 1;
      cb.last  = c;
      cb.count = 1;
    }
}

// runlength encode the raw base64 buffer
static void
flush_base64_buffers(void)
{
  int nout = print_base64(buffer, buf_idx, outbuf);
  for (int i = 0; i < nout; i++)
    rle(outbuf[i]);
}

// store a 32 bit number
void
store_gcov_u32(u32 v)
{
  const char *cp = (char *)&v;
  for (int i = 0; i < 4; i++)
    buffer[buf_idx++] = cp[i];

  if (buf_idx == BUF_SIZE)
    {
      flush_base64_buffers();
      buf_idx = 0;
    }
}

void
output_gcov_data(struct gcov_info *tmp)
{
  vconprint("@@ gcov @< BLOCK\n");
  while (tmp)
    {
      vconprint("FILE '");
      vconprint(tmp->filename);
      vconprint("' ZDATA64 '");
      convert_to_gcda(tmp);
      flush_base64_buffers();
      buf_idx = 0;
      rle_flush();
      vconprint("'\n");
      tmp = tmp->next;
    }
  vconprint("@@ gcov BLOCK >@\n");
}
