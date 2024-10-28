/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Valentin Gehrke <valentin.gehrke@kernkonzept.com>
 *
 */
#include "output.h"

void
cov_outputn(char const *s, int len)
{
  vconprintn(s, len);
}

void
cov_output(char const *s)
{
  vconprint(s);
}
