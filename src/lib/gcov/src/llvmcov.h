/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020, 2023 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */

#pragma once

#include <globalconfig.h>

/**
 * Process data from llvm coverage profiling for output
 */
void
output_llvmcov_data(void);

/**
 * Triggers base64 coverage output for llvm coverage.
 */
void
dump_coverage(void);

/**
 * write back a blob of data in base64 format.
 *
 * \param data the actual data to encode.
 * \param elem_size size of one element.
 * \param n_elem number of elements.
 *
 * The function returns the number of processed bytes.
 */
unsigned
store_b64(void const *data, unsigned elem_size, unsigned n_elem);

#ifdef CONFIG_AMP
# define GLOBAL_DATA __attribute__((section(".global_data")))
#else
# define GLOBAL_DATA
#endif
