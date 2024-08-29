/*
 * Copyright 2023-2024 NXP
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

INTERFACE [arm && pf_s32n5]:

#define TARGET_NAME "NXP S32N5"

// ----------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32n && pf_s32n_rtu_cl0_split && pf_s32n_rtu_cl1_split]:

#include "minmax.h"

EXTENSION class Config
{
public:
  enum { First_node = 0, Max_num_nodes = min<int>(4, Max_num_cpus) };
};

// ----------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32n && (pf_s32n_rtu_cl0_split && pf_s32n_rtu_cl1_lockstep) || (pf_s32n_rtu_cl1_split && pf_s32n_rtu_cl0_lockstep)]:

#include "minmax.h"

EXTENSION class Config
{
public:
  enum { First_node = 0, Max_num_nodes = min<int>(3, Max_num_cpus) };
};

// ----------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32n && (pf_s32n_rtu_cl0_split && pf_s32n_rtu_cl1_disabled) || (pf_s32n_rtu_cl0_lockstep && pf_s32n_rtu_cl1_lockstep)]:

#include "minmax.h"

EXTENSION class Config
{
public:
  enum { First_node = 0, Max_num_nodes = min<int>(2, Max_num_cpus) };
};

// ----------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32n && pf_s32n_rtu_cl0_disabled && pf_s32n_rtu_cl1_split]:

#include "minmax.h"

EXTENSION class Config
{
public:
  enum { First_node = 2, Max_num_nodes = min<int>(2, Max_num_cpus) };
};
