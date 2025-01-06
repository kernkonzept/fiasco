/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020, 2023 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 */
#pragma once

enum
{
  BUF_SIZE = 192
};

extern char buffer[BUF_SIZE];
extern char outbuf[BUF_SIZE * 4 / 3];
extern char const *b64;

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
print_base64(char const *s, int l, char *out);

/**
 * print the current state of the base 64 buffers and reset them.
 */
void
flush_base64_buffers(void);

/**
 * Store 32 bit number in cov format to buffer
 *
 * \param v value to be stored
 *
 * Number format defined by gcc: numbers are recorded in the 32 bit
 * unsigned binary form of the endianness of the machine generating the
 * file.
 */
void
store_u32(unsigned long v);

/**
 * Store any amount of data in cov format to buffer.
 *
 * \param v array of data to be stored.
 * \param length number of bytes to store from v.
 */
void
store(const void *v, unsigned length);
