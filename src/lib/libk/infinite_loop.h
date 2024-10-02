#pragma once

#include "std_macros.h"

namespace L4 {

/**
 * Spin and never return.
 */
[[noreturn]] inline void infinite_loop()
{
  while (1)
    asm volatile ("" ::: "memory");
}

}
