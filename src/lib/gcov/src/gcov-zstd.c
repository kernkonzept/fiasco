/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020, 2022-2023 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */
#include "base64.h"
#include "output.h"
#include "gcov.h"
#include "zstd.h"

extern unsigned outOffset;

void
output_gcov_data(struct gcov_info *tmp)
{
  init_zstd();
  struct gcov_info *cur = tmp;

  cov_output("@@ gcov: ZSTD '");
  while (cur)
    {
      convert_to_gcda(cur);
      cur->stamp = outOffset; // abuse stamp field to save the file length
      cur        = cur->next;
    }

  // Write the trailing header
  // Format:
  //   ([length:uint][fname:zstring])*files[header offset:uint]
  cur                = tmp;
  unsigned headStart = outOffset;
  while (cur)
    {
      store_u32(cur->stamp); // store offset
      store_string(cur->filename, ZSTD_e_continue);
      cur = cur->next;
    }
  store_u32(headStart);
  store_string("", ZSTD_e_end); // Flush zstd buffers
  flush_base64_buffers();
  cov_output("'\n");
}
