/*
 * Copyright 2023-2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

IMPLEMENTATION [arm && pf_s32n]:

#include "infinite_loop.h"

[[noreturn]] void
platform_reset(void)
{
  L4::infinite_loop();
}
