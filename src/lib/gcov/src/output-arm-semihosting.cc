/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Valentin Gehrke <valentin.gehrke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

extern "C" {
#include "output.h"
}

#include "llvmcov.h"

#include <panic.h>
#include <static_init.h>
#include <semihosting.h>

static GLOBAL_DATA Semihosting::File_handle handle =
  Semihosting::Invalid_file_handle;

static void
cov_semihosting_init()
{
  handle = Semihosting::sys_open(Semihosting::Magic_stdio_path,
                                 Semihosting::Mode_write);

  if (handle == Semihosting::Invalid_file_handle)
    panic("COVERAGE: Failed to open semihosting handle for stdout");

  cov_output("COVERAGE: Hello from the other side!\n");
}

STATIC_INITIALIZER(cov_semihosting_init);

extern "C"
void
cov_outputn(char const *s, int len)
{
  Semihosting::sys_write(handle, s, len);
}

extern "C"
void
cov_output(char const *s)
{
  cov_outputn(s, __builtin_strlen(s));
}
