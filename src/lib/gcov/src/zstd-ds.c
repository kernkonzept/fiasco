/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/rm.h>
#include "output.h"
#include "zstd.h"

void *zstd_workspace;
int zstd_workspace_size;

// Use the dataspace API to allocate enough space to hold the ZSTD workspace
int
init_storage_backend(void)
{
  zstd_workspace_size = ZSTD_estimateCCtxSize(CompressionLevel) * 2;
  l4re_ds_t ds        = l4re_util_cap_alloc();

  if (l4_is_invalid_cap(ds))
    {
      vconprint("COV: ERROR: Could not allocate cap\n");
      return -1;
    }

  if (l4re_ma_alloc(zstd_workspace_size, ds, 0))
    {
      vconprint("COV: ERROR: Could not allocate\n");
      return -1;
    }

  if (l4re_rm_attach(&zstd_workspace, zstd_workspace_size, L4RE_RM_F_SEARCH_ADDR | L4RE_RM_F_RW,
                     ds, 0, L4_SUPERPAGESHIFT))
    {
      vconprint("COV: ERROR: Could attach DS\n");
      return -1;
    }
  return 0;
}

// we use default compression parameters when using a dataspace buffer
void
set_compression_parameters(ZSTD_CCtx *ctx)
{ (void)ctx; }
