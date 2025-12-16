/*
 * Copyright 2023-2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

// ------------------------------------------------------------------------
INTERFACE [arm && pf_s32n5]:

EXTENSION class Mem_layout
{
public:
  enum Mpu_layout { Mpu_regions = 24 };
};

// ------------------------------------------------------------------------
INTERFACE [arm && pf_s32n5 && pf_s32n_rtu_0]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_s32: Address
  {
    Gic_phys_base      = 0x4b000000,
    Rtu_Mru0           = 0x50470000,
    Rtu_Mru1           = 0x505c0000,
    Rtu_Mru2           = 0x50a20000,
    Rtu_Mru3           = 0x50c00000,

    // Map whole GIC
    Gic_phys_size      =   0x200000,
    Gic_redist_offset  =   0x100000,
    Gic_redist_size    =   0x100000,

    Mru_size           =    0x10000,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && pf_s32n5 && pf_s32n_rtu_1]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_s32: Address
  {
    Gic_phys_base      = 0x4b400000,
    Rtu_Mru0           = 0x51470000,
    Rtu_Mru1           = 0x515c0000,
    Rtu_Mru2           = 0x51a20000,
    Rtu_Mru3           = 0x51c00000,

    // Map whole GIC
    Gic_phys_size      =   0x200000,
    Gic_redist_offset  =   0x100000,
    Gic_redist_size    =   0x100000,

    Mru_size           =    0x10000,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && pf_s32n5 && pf_s32n_rtu_2]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_s32: Address
  {
    Gic_phys_base      = 0x4b800000,
    Rtu_Mru0           = 0x52470000,
    Rtu_Mru1           = 0x525c0000,
    Rtu_Mru2           = 0x52a20000,
    Rtu_Mru3           = 0x52c00000,

    // Map whole GIC
    Gic_phys_size      =   0x200000,
    Gic_redist_offset  =   0x100000,
    Gic_redist_size    =   0x100000,

    Mru_size           =    0x10000,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && pf_s32n5 && pf_s32n_rtu_3]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_s32: Address
  {
    Gic_phys_base      = 0x4bc00000,
    Rtu_Mru0           = 0x53470000,
    Rtu_Mru1           = 0x535c0000,
    Rtu_Mru2           = 0x53a20000,
    Rtu_Mru3           = 0x53c00000,

    // Map whole GIC
    Gic_phys_size      =   0x200000,
    Gic_redist_offset  =   0x100000,
    Gic_redist_size    =   0x100000,

    Mru_size           =    0x10000,
  };
};
