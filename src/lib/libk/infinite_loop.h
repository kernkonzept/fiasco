#pragma once

#include "std_macros.h"

namespace L4 {

/**
 * Spin and never return.
 */
inline void FIASCO_NORETURN infinite_loop()
{
  while (1)
    asm volatile ("" ::: "memory");
}

}
