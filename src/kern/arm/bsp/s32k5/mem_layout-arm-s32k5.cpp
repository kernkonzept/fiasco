/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

// ----------------------------------------------------------------------------
INTERFACE [arm && pf_s32k5]:

EXTENSION class Mem_layout
{
public:
  enum Mpu_layout { Mpu_regions = 20 };

  enum Phys_layout_s32: Address
  {
    Gic_phys_base      = 0x43000000,

    /* Map whole GIC */
    Gic_phys_size      =   0x200000,
    Gic_redist_offset  =   0x100000,
    Gic_redist_size    =   0x100000,
  };
};
