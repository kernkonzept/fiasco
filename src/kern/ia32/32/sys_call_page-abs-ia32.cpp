/*
 * Fiasco Syscall-Page Code (absolute addressing)
 */

IMPLEMENTATION [ia32-abs_syscalls]:

#include <cstdio>
#include <cstring>
#include "config.h"
#include "cpu.h"
#include "kernel_task.h"
#include "mem_layout.h"
#include "panic.h"
#include "paging.h"
#include "space.h"
#include "types.h"
#include "vmem_alloc.h"

enum
{
  Offs_invoke            = 0x000,
  Offs_se_invoke         = 0x000,
  Offs_kip_invoke        = 0x800,
  Offs_kip_se_invoke     = 0x800,
  Offs_debugger          = 0x200,
  Offs_kip_debugger      = 0x900,
};


#define INV_SYSCALL(sysc) \
  *reinterpret_cast<Unsigned16*>(Mem_layout::Syscalls + Offs_##sysc) = 0x0b0f

#define SYSCALL_SYMS(sysc) \
extern char sys_call_##sysc, sys_call_##sysc##_end

#define COPY_SYSCALL(sysc) do { \
memcpy( (char*)Mem_layout::Syscalls + Offs_##sysc, &sys_call_##sysc, \
        &sys_call_##sysc##_end- &sys_call_##sysc ); \
memcpy( (char*)Kip::k() + Offs_kip_##sysc, &sys_call_##sysc, \
        &sys_call_##sysc##_end- &sys_call_##sysc ); } while (0)


IMPLEMENT 
void
Sys_call_page::init()
{
  SYSCALL_SYMS(invoke);
  SYSCALL_SYMS(se_invoke);
  SYSCALL_SYMS(debugger);

  if (!Vmem_alloc::page_alloc((void*)Mem_layout::Syscalls, 
	Vmem_alloc::ZERO_FILL, Vmem_alloc::User))
    panic("FIASCO: can't allocate system-call page.\n");

  printf ("Absolute KIP Syscalls using: %s\n",
          Cpu::have_sysenter() ? "Sysenter" : "int 0x30");

  Kip::k()->kip_sys_calls       = 2;

  if (Cpu::have_sysenter())
    COPY_SYSCALL(se_invoke);
  else
    COPY_SYSCALL(invoke);

  COPY_SYSCALL(debugger);

  Kernel_task::kernel_task()->set_attributes(
      Virt_addr(Mem_layout::Syscalls),
      Page::Attr(Page::Rights::UR(), Page::Type::Normal(),
                 Page::Kern::Global()));
}
