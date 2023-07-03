/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */
#pragma once
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
