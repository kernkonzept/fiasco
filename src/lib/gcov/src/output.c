/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020, 2023 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */
#include "output.h"
#include <stdio.h>

void
vconprintn(char const *s, int len)
{
  printf("%.*s", len, s);
}

void
vconprint(char const *s)
{
  vconprintn(s, __builtin_strlen(s));
}



