/*
 * Copyright 2023-2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Core configuration for AMP:
 *
 *   0   1   2   3     first  max
 *   s   s   s   s     0      4
 *   s   s   _ls__     0      3
 *   _ls__   s   s     0      3
 *   _ls__   _ls__     0      2
 *   s   s   -   -     0      2
 *   -   -   s   s     2      2
 *
 *   s: split-lock, ls: lock-step, -: disabled
 *   all other combinations don't apply for AMP
 */

// ------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32n && pf_s32n_rtu_cl0_split && pf_s32n_rtu_cl1_split]:

EXTENSION class Amp_node
{
public:
  static constexpr unsigned Max_cores = 4;
};

// ------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32n && pf_s32n_rtu_cl0_split && pf_s32n_rtu_cl1_lockstep]:

EXTENSION class Amp_node
{
public:
  static constexpr unsigned Max_cores = 3;
};

// ------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32n && pf_s32n_rtu_cl0_lockstep && pf_s32n_rtu_cl1_split]:

EXTENSION class Amp_node
{
public:
  static constexpr unsigned Max_cores = 3;
};


// ------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32n && pf_s32n_rtu_cl0_lockstep && pf_s32n_rtu_cl1_lockstep]:

EXTENSION class Amp_node
{
public:
  static constexpr unsigned Max_cores = 2;
};

// ------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32n && pf_s32n_rtu_cl0_split && pf_s32n_rtu_cl1_disabled]:

EXTENSION class Amp_node
{
public:
  static constexpr unsigned Max_cores = 2;
};

// ------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32n && pf_s32n_rtu_cl0_disabled && pf_s32n_rtu_cl1_split]:

EXTENSION class Amp_node
{
public:
  static constexpr unsigned Max_cores = 2;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32n && pf_s32n_rtu_cl0_disabled]:

IMPLEMENT_OVERRIDE static constexpr
Amp_phys_id
Amp_node::first_node()
{ return Amp_phys_id(2); }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32n]:

#include "processor.h"

IMPLEMENT inline ALWAYS_INLINE NEEDS["processor.h"]
FIASCO_PURE Amp_phys_id
Amp_node::phys_id()
{
  // Aff0: 0..1 - core in cluster
  // Aff1: 0..1 - cluster in RTU
  // Aff2: 0..3 - RTU in SoC, not used since we run in a single RTU
  unsigned mpidr = cxx::int_value<Cpu_phys_id>(Proc::cpu_id());
  return Amp_phys_id(((mpidr & 0x100) >> 7) | (mpidr & 0x1));
}
