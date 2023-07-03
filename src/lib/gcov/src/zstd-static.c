/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */
#include "zstd.h"
// Note: WORKSPACE must be at least 1<<WINDOW_LOG+4 (e.g. 256kB for 14)
// Recommended: 14 and 256
enum
{
  WindowLog     = 12,
  WorkspaceSize = 4096 << 10,
};

static char
workspace[WorkspaceSize] __attribute__ ((aligned (__BIGGEST_ALIGNMENT__)));

void *
zstd_workspace    = &workspace;
int
zstd_workspace_size = WorkspaceSize;

int
init_storage_backend(void)
{
  __builtin_memset(workspace, '\0', WorkspaceSize);
  return 0;
}

void
set_compression_parameters(ZSTD_CCtx *ctx)
{
  ZSTD_CCtx_setParameter(ctx, ZSTD_c_windowLog, WindowLog);
  ZSTD_CCtx_setParameter(ctx, ZSTD_c_chainLog, ZSTD_CHAINLOG_MIN);
}

void *__attribute__((weak)) mmap;
void *__attribute__((weak)) mmap;
void *__attribute__((weak)) uclibc_morecore;
void *__attribute__((weak)) munmap;
#ifdef __clang__
void *__attribute__((weak)) malloc(size_t);
void *__attribute__((weak)) memmove(void *, const void *, size_t);
void  __attribute__((weak)) free(void *);
void *__attribute__((weak)) calloc(size_t, size_t);
#else
void *__attribute__((weak)) malloc;
void *__attribute__((weak)) memmove;
void *__attribute__((weak)) free;
void *__attribute__((weak)) calloc;
#endif
