/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

IMPLEMENTATION [arm && pf_s32k5]:

#include "infinite_loop.h"

[[noreturn]] void
platform_reset(void)
{
  L4::infinite_loop();
}
