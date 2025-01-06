/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020, 2023-2024 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */
#pragma once
/**
 * Output coverage data according to the selected output.
 *
 * \param s The zero terminated string to print.
 */
void
cov_output(char const *s);

/**
 * Output coverage data according to the selected output.
 *
 * \param s    Pointer to the character array from which to print.
 * \param len  The number of characters to print from s.
 */
void
cov_outputn(char const *s, int len);

/**
 * Output a zero terminated string to vcon, e.g. errors.
 *
 * \param s The zero terminated string to print.
 */
void
vconprint(char const *s);

/**
 * Output a defined number of characters to vcon, e.g. errors.
 *
 * \param s    Pointer to the character array from which to print.
 * \param len  The number of characters to print from s.
 */
void
vconprintn(char const *s, int len);
