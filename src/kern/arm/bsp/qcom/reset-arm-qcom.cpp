/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2022 Stephan Gerhold <stephan@gerhold.net>
 * Copyright (C) 2022-2023 Kernkonzept GmbH.
 */

IMPLEMENTATION [arm && pf_qcom && arm_psci]:

#include "psci.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Psci::system_reset();
  for (;;)
    ;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_qcom && !arm_psci]:

#include "io.h"
#include "kmem.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Address base = Kmem::mmio_remap(Mem_layout::Mpm_ps_hold, sizeof(Unsigned32));
  Io::write<Unsigned32>(0, base);
  for (;;)
    ;
}
