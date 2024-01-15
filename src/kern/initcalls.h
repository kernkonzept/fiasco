#ifndef INITCALLS_H__
#define INITCALLS_H__

#include "globalconfig.h"

/*
 * Code and data placed into the .initcall.text* and .initcall.data sections
 * is discarded once the kernel is initialized.
 */

#define FIASCO_INIT             __attribute__((section(".initcall.text")))
#define FIASCO_INIT_SFX(suffix) __attribute__((section(".initcall.text." #suffix)))
#define FIASCO_INITDATA         __attribute__((section(".initcall.data")))

#define FIASCO_INIT_AND_PM
#define FIASCO_INIT_AND_PM_DATA

#define FIASCO_INIT_CPU_AND_PM
#define FIASCO_INIT_CPU_AND_PM_DATA

#ifdef CONFIG_MP

/*
 * On multi-processor builds, we cannot discard the CPU initialization code
 * and data since it might be needed at any time for CPU hot-plug.
 */

#define FIASCO_INIT_CPU
#define FIASCO_INIT_CPU_SFX(suffix)
#define FIASCO_INITDATA_CPU

#else // CONFIG_MP

/**
 * Place CPU initialization code symbol into the .initcall.text section.
 *
 * This enables the code to be discarded once the CPU has been initialized.
 */
#define FIASCO_INIT_CPU FIASCO_INIT

/**
 * Place CPU initialization code symbol into the .initcall.text.suffix section.
 *
 * This enables the code to be discarded once the CPU has been initialized.
 *
 * The section suffix enables to garbage-collect the symbol by the linker
 * in case it is not needed in the final binary. It is also a workaround for
 * a compiler limitation related to inline functions (the section containing
 * non-static inline functions has conflicting attributes with the section
 * containing regular functions).
 *
 * \param suffix  Section suffix.
 */
#define FIASCO_INIT_CPU_SFX(suffix) FIASCO_INIT_SFX(suffix)

/**
 * Place CPU initialization object symbol into the .initcall.data section.
 */
#define FIASCO_INITDATA_CPU FIASCO_INITDATA

#endif // CONFIG_MP

#endif // INITCALLS_H__
