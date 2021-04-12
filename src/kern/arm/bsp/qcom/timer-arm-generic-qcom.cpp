/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2022 Stephan Gerhold <stephan@gerhold.net>
 * Copyright (C) 2022-2023 Kernkonzept GmbH.
 */

IMPLEMENTATION [arm_generic_timer && pf_qcom && have_arm_gicv2]:

PUBLIC static
unsigned Timer::irq()
{
  switch (Gtimer::Type)
    {
    case Generic_timer::Physical: return 16 + 3; // (non-secure)
    case Generic_timer::Virtual:  return 16 + 4;
    case Generic_timer::Hyp:      return 16 + 1;
    };
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm_generic_timer && pf_qcom && pf_qcom_sm8150]:

PUBLIC static
unsigned Timer::irq()
{
  switch (Gtimer::Type)
    {
    case Generic_timer::Physical: return 16 + 2; // (non-secure)
    case Generic_timer::Virtual:  return 16 + 3;
    case Generic_timer::Hyp:      return 16 + 0;
    };
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm_generic_timer && pf_qcom]:

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{}
