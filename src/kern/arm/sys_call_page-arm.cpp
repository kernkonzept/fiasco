INTERFACE:

#include "types.h"

//----------------------------------------------------------------------------
IMPLEMENTATION[arm && !arm_v6plus]:

PRIVATE static inline NOEXPORT NEEDS["types.h"]
void
Sys_call_page::set_utcb_get_code(Unsigned32 *sys_calls)
{
  unsigned offset = 0;
  // r0 = [Mem_layout::Utcb_ptr_page]
  sys_calls[offset++] = 0xe3e00a02; // mvn r0, #8192
  sys_calls[offset++] = 0xe5100fff; // ldr r0, [r0, -#4095]
  sys_calls[offset++] = 0xe1a0f00e; // mov pc, lr

  // set TLS REG
  offset = 0x20;
  sys_calls[offset++] = 0xe1a0f00e; // mov pc, lr
}

//----------------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v6plus]:

PRIVATE static inline NOEXPORT NEEDS["types.h"]
void
Sys_call_page::set_utcb_get_code(Unsigned32 *sys_calls)
{
  unsigned offset = 0;

  // sys_calls[0] = 0xee1d0f70; // mrc 15, 0, r0, cr13, cr0, {3}
  sys_calls[offset++] = 0xee1d0f50; // mrc 15, 0, r0, cr13, cr0, {2}
  sys_calls[offset++] = 0xe1a0f00e; // mov pc, lr

  // set TLS REG
  offset = 0x20;
  sys_calls[offset++] = 0xe92d4010; // push    {r4, lr}
  sys_calls[offset++] = 0xee1d4f50; // mrc     15, 0, r4, cr13, cr0, {2}
  sys_calls[offset++] = 0xe5840004; // str     r0, [r4, #4]
  sys_calls[offset++] = 0xe3a02010; // mov     r2, #0x10 -> set tls opcode
  sys_calls[offset++] = 0xe5842000; // str     r2, [r4]
  sys_calls[offset++] = 0xe3a03000; // mov     r3, #0
  sys_calls[offset++] = 0xe3e020fc; // mvn     r2, #252
  sys_calls[offset++] = 0xe3c22c07; // bic     r2, r2, #1792
  sys_calls[offset++] = 0xe59f0008; // ldr     r0, [pc, #8]
  sys_calls[offset++] = 0xe1a0e00f; // mov     lr, pc
  sys_calls[offset++] = 0xe3e0f00b; // mvn     pc, #11
  sys_calls[offset++] = 0xe8bd8010; // pop     {r4, pc}
  sys_calls[offset++] = 0xfff40002; // .word   0xfff40002
}

//----------------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v5]:

PRIVATE static inline NOEXPORT NEEDS["types.h"]
void
Sys_call_page::set_dmb(Unsigned32 *m)
{
  // There is no DMB on ARMv5 and DSB (back then called "Drain Write Buffer")
  // is not available from user mode (MCR is a privileged instruction on ARMv5)
  m[0] = 0xe1a0f00e; // mov pc, lr
}

//----------------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v6]:

PRIVATE static inline NOEXPORT NEEDS["types.h"]
void
Sys_call_page::set_dmb(Unsigned32 *m)
{
  m[0] = 0xee070fba; // mcr 15, 0, r0, cr7, cr10, {5}
  m[1] = 0xe1a0f00e; // mov pc, lr
}

//----------------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v7plus]:

PRIVATE static inline NOEXPORT NEEDS["types.h"]
void
Sys_call_page::set_dmb(Unsigned32 *m)
{
  m[0] = 0xf57ff05f; // dmb sy
  m[1] = 0xe12fff1e; // bx lr
}

//----------------------------------------------------------------------------
IMPLEMENTATION:

#include <cstring>
#include "kernel_task.h"
#include "mem_layout.h"
#include "vmem_alloc.h"
#include "panic.h"

IMPLEMENT static
void
Sys_call_page::init()
{
  Unsigned32 *sys_calls =
    static_cast<Unsigned32*>(
      Kmem_alloc::allocator()->alloc(Config::page_size()));
  if (!sys_calls)
    panic("Can't allocate system-call page.");

  // We don't use the Sys_call_page as trampoline anymore. Instead, raise an
  // exception IPC if some old code still jumps here.
  for (unsigned i = 0; i < Config::PAGE_SIZE / sizeof(Unsigned32); ++i)
    sys_calls[i] = 0xe1200070; // bkpt #0

  set_utcb_get_code(sys_calls + (0xf00 / sizeof(Unsigned32)));

  set_dmb(sys_calls + (0xf40 / sizeof(Unsigned32)));

  Mem_unit::flush_cache();

  Kernel_task::map_syscall_page(sys_calls);
}
