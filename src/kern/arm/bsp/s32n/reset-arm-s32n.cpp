/*
 * Copyright 2023-2024 NXP
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

IMPLEMENTATION [arm && pf_s32n]:

#include "mmio_register_block.h"
#include "kmem.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  for (;;)
    ;
}
