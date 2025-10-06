#pragma once

#include "stdio_impl.h"

size_t __libc_stdout_write(FILE *f, const char *buf, size_t len);
