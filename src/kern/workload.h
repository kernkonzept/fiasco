#pragma once

#include "globalconfig.h"

#ifdef CONFIG_BIT64
# define FN_PTR ".quad"
#else
# define FN_PTR ".long"
#endif

/**
 * Mark this function as workload for the boot CPU.
 *
 * \param priority  Priority of the function: A higher priority indicated by a
 *                  lower number means earlier execution. A lower priority
 *                  indicated by a higher number means later execution. The
 *                  standard workload uses INIT_WORKLOAD_PRIO_STD. The priority
 *                  is appended to the section name and the SORT_BY_INIT_PRIO()
 *                  linker script command instructs the linker to sort the
 *                  function pointers accordingly.
 * \param function  Function to be called.
 *
 * This function is called from Kernel_thread::init_workload().
 */
#define INIT_WORKLOAD(priority, function)               \
  asm (".pushsection .init_workload." # priority ", \"a\" \n\t" \
       FN_PTR " " #function "                             \n\t" \
       ".popsection                                       \n\t")

/**
 * Mark this function as workload for an application CPU.
 *
 * \param priority  Same meaning as with INIT_WORKLOAD.
 * \param function  Function to be called.
 *
 * This function is called from App_cpu_thread::init_workload().
 */
#define INIT_WORKLOAD_APP_CPU(priority, function)                       \
  asm (".pushsection .init_workload_app_cpu." # priority ", \"a\" \n\t" \
       FN_PTR " " #function "                                     \n\t" \
       ".popsection                                               \n\t")

/**
 * Initialization priority of the standard kernel workload.
 *
 * Use a number > 0 so that other workloads may be created before.
 */
#define INIT_WORKLOAD_PRIO_STD                  10

/**
 * Initialization priority of the unit tests.
 *
 * If unit tests are active, the standard kernel workload is not created!
 * Usually no other workload is executed after a unit test.
 */
#define INIT_WORKLOAD_PRIO_UNIT_TEST            99
