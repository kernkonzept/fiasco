#pragma once

#include "globalconfig.h"

// no memory mapped at 0...FIASCO_MP_TRAMP_PAGE
#define FIASCO_MP_TRAMP_PAGE     0x8000  // must be below 1 MB
#define FIASCO_BDA_PAGE          (FIASCO_MP_TRAMP_PAGE + 0x1000)
#define FIASCO_IMAGE_PHYS_START  0x400000
//#define FIASCO_IMAGE_PHYS_START  0x2000
#define FIASCO_IMAGE_VIRT_START  0xfffffffff0000000

// must be superpage-aligned
#if !defined(CONFIG_KERNEL_NX) && !defined(CONFIG_COV)
#define FIASCO_IMAGE_VIRT_SIZE   0x400000
#elif !defined(CONFIG_COV)
#define FIASCO_IMAGE_VIRT_SIZE   0x600000
#else
#define FIASCO_IMAGE_VIRT_SIZE   0x800000
#endif

#define FIASCO_IMAGE_PHYS_OFFSET (FIASCO_IMAGE_VIRT_START - (FIASCO_IMAGE_PHYS_START & 0xffffffffffc00000))
