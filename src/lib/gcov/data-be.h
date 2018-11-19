// SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom
/*
 * Copyright (C) 2019 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 */
#pragma once
#include "gcov.h"

// This file contains the declarations for functions required to process
// coverage data

/**
 * Store 32 bit number in gcov format to buffer
 *
 * \param v value to be stored
 *
 * Number format defined by gcc: numbers are recorded in the 32 bit
 * unsigned binary form of the endianness of the machine generating the
 * file.
 */
void store_gcov_u32(u32 v);

/**
 * Process data from gcov_info for output
 *
 * \param tmp The gcov_info data that shall be output.
 *
 * The main function that must be implemented by a output backend to process
 * the gcov data.
 *
 */
void output_gcov_data(struct gcov_info *tmp);
