/*
 * Fiasco Syscall-Page Code (absolute addressing)
 * Architecture specific UX code.
 */

IMPLEMENTATION [ux-abs_syscalls]:

#include "kmem.h"
#include "mem_layout.h"
#include "types.h"
#include <cstring>

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
memcpy( (char*)Kip::k() + Offs_kip_##sysc, &sys_call_##sysc, \
        &sys_call_##sysc##_end- &sys_call_##sysc ); } while (0)

IMPLEMENT 
void
Sys_call_page::init()
{
  SYSCALL_SYMS(invoke);
  SYSCALL_SYMS(debugger);

  Kip *ki = Kip::k();

  ki->kip_sys_calls       = 2;

  COPY_SYSCALL(invoke);
  COPY_SYSCALL(debugger);
}
