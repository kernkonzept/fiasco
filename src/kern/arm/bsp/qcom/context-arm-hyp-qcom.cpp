/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Stephan Gerhold <stephan.gerhold@kernkonzept.com>
 */

IMPLEMENTATION [arm && cpu_virt && pf_qcom && have_arm_gicv2]:

IMPLEMENT_OVERRIDE inline
unsigned Hyp_irqs::vgic()
{ return 16 + 0; }

IMPLEMENT_OVERRIDE inline
unsigned Hyp_irqs::vtimer()
{ return 16 + 4; }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && pf_qcom_sm8150]:

IMPLEMENT_OVERRIDE inline
unsigned Hyp_irqs::vtimer()
{ return 16 + 3; }
