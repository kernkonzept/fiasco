/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2019-2022 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */

#include "output.h"
#include "base64.h"
#include "gcov.h"
#include <stddef.h>

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
      vconprint("'\n");
      tmp = tmp->next;
    }
  vconprint("@@ gcov BLOCK >@\n");
}
