/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2019-2024 Kernkonzept GmbH.
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
  cov_output("@@ gcov @< BLOCK\n");
  while (tmp)
    {
      cov_output("FILE '");
      cov_output(tmp->filename);
      cov_output("' ZDATA64 '");
      convert_to_gcda(tmp);
      flush_base64_buffers();
      cov_output("'\n");
      tmp = tmp->next;
    }
  cov_output("@@ gcov BLOCK >@\n");
}
