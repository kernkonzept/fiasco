/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */
#pragma once
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

// This should not be configured!
enum { CompressionLevel = 5 };

extern void* zstd_workspace;
extern int   zstd_workspace_size;

int
init_storage_backend(void);

void
set_compression_parameters(ZSTD_CCtx* ctx);

void
init_zstd(void);

void
store_string(char const *s, ZSTD_EndDirective mode);
