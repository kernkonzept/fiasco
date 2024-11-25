/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 */

IMPLEMENTATION [mips]:

#include "mem_layout.h"

IMPLEMENT inline
void
Kernel_thread::free_initcall_section()
{
  memset(const_cast<char *>(Mem_layout::initcall_start), 0,
         Mem_layout::initcall_size());
  printf("%zu KB kernel memory freed @ %p\n", Mem_layout::initcall_size() / 1024,
         Mem_layout::initcall_start);
}

IMPLEMENT FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  boot_app_cpus();
}

//--------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

PUBLIC
static inline void
Kernel_thread::boot_app_cpus()
{}

//--------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "platform_control.h"

PUBLIC
static void
Kernel_thread::boot_app_cpus()
{
  if constexpr (Config::Max_num_cpus > 1)
    Platform_control::boot_all_secondary_cpus();
}
