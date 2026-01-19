/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2024-2025 Kernkonzept GmbH.
 * Author(s): Valentin Gehrke <valentin.gehrke@kernkonzept.com
 *
 */
extern "C" {
#include "output.h"
}

#include <cstdio>
#include <cstring>

#include <panic.h>
#include <globalconfig.h>
#include <kmem_mmio.h>
#include <static_init.h>

#include "llvmcov.h"

static GLOBAL_DATA char *membuf_ptr = nullptr;
static GLOBAL_DATA char const *membuf_end = nullptr;

static void
cov_membuf_init()
{
  const Address membuf_start_phys = CONFIG_COV_OUTPUT_MEMBUF_START;
  const size_t membuf_size = CONFIG_COV_OUTPUT_MEMBUF_SIZE;

  membuf_ptr = static_cast<char*>(Kmem_mmio::map(membuf_start_phys, membuf_size,
                                                 Kmem_mmio::Map_attr::Cached()));
  membuf_end = &membuf_ptr[membuf_size];

  printf("COVERAGE: Using memory buffer at %#lx - %#lx (size %#zx)\n",
         membuf_start_phys, membuf_start_phys + membuf_size, membuf_size);
}

STATIC_INITIALIZER(cov_membuf_init);

extern "C"
void
cov_outputn(char const *s, int len)
{
  if (membuf_ptr + len >= membuf_end)
    panic("Coverage memory buffer full. Please increase CONFIG_COV_OUTPUT_MEMBUF_SIZE.");

  memcpy(membuf_ptr, s, len);

  membuf_ptr += len;

  // Add null-terminator at end of buffer, so we know how full the memory buffer
  // is after dumping it. Will be overriden with the next call of cov_outputn
  *membuf_ptr = '\0';
}

extern "C"
void
cov_output(char const *s)
{
  cov_outputn(s, __builtin_strlen(s));
}
