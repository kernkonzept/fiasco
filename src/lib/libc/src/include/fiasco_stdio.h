#pragma once

#include "stdio_impl.h"

size_t __fiasco_stdout_write(FILE *f, const unsigned char *buf, size_t len);
