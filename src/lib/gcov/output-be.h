// SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom
/*
 * Copyright (C) 2019 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 */
#pragma once
#include <stddef.h>
#include "gcov.h"
// This header contains the declarations for functions needed to print data
/**
 * Output a zero terminated string
 *
 * \param s The zero terminated string to print.
 */
void
vconprint(char const *s);
/**
 * Output a defined number of characters
 *
 * \param s    Pointer to the character array from which to print.
 * \param len  The number of characters to print from s.
 */
void
vconprintn(char const *s, int len);

/**
 * Outputs the gcov data from a gcov_info structure
 *
 * \param info Pointer to the gcov_info structure with the data to output
 */
void
convert_to_gcda(struct gcov_info *info);

/**
 * Encode a character string into base64
 *
 * \param s        The source string to encode.
 * \param l        The number of characters from the source string to encode.
 * \param out[out] The destination buffer.
 *                 The size of the buffer must be at least ceil(l/3)*4 for it
 *                 to be able to hold all data. Behavior is undefined if the
 *                 buffer is too small.
 *
 * The function returns the number of characters written to the out buffer.
 */
long
print_base64(const char *s, int l, char *out);

