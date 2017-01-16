IMPLEMENTATION [arm && !cpu_virt]:

#include <panic.h>

#include "kmem_alloc.h"
#include "kmem.h"
#include "ram_quota.h"
#include "paging.h"
#include "static_init.h"

class Kern_lib_page
{
public:
  static void init();
};

IMPLEMENT_DEFAULT
void Kern_lib_page::init()
{
  extern char kern_lib_start;
  auto pte = Kmem::kdir->walk(Virt_addr(Kmem_space::Kern_lib_base),
                              Pdir::Depth, true,
                              Kmem_alloc::q_allocator(Ram_quota::root));

  if (pte.level == 0) // allocation of second level faild
    panic("FATAL: Error mapping kernel-lib page to %p\n",
          (void *)Kmem_space::Kern_lib_base);

  pte.set_page(pte.make_page(Phys_mem_addr((Address)&kern_lib_start - Mem_layout::Map_base
                                           + Mem_layout::Sdram_phys_base),
                             Page::Attr(Page::Rights::URX(), Page::Type::Normal(),
                                        Page::Kern::Global())));
  pte.write_back_if(true, Mem_unit::Asid_kernel);
}

STATIC_INITIALIZE(Kern_lib_page);

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_v6plus]:

asm (
    ".p2align(12)                        \n"
    "kern_lib_start:                     \n"

    // atomic add
    // r0: memory reference
    // r1: delta value
    "  ldr r2, [r0]			 \n"
    "  add r2, r2, r1                    \n"
    "  nop                               \n"
    "  str r2, [r0]                      \n"
    // forward point
    "  mov r0, r2                        \n"
    "  mov pc, lr                        \n"
    // return: always succeeds, new value

    // compare exchange
    // r0: memory reference
    // r1: cmp value
    // r2: new value
    // r3: tmp
    ".p2align(8)                         \n"
    "  ldr r3, [r0]			 \n"
    "  cmp r3, r1                        \n"
    "  nop                               \n"
    "  streq r2, [r0]                    \n"
    // forward point
    "  moveq r0, #1                      \n"
    "  movne r0, #0                      \n"
    "  mov pc, lr                        \n"
    // return result: 1 success, 0 failure

    // exchange
    //  in-r0: memory reference
    //  in-r1: new value
    // out-r0: old value
    // tmp-r2
    ".p2align(8)                         \n"
    "  ldr r2, [r0]			 \n"
    "  nop                               \n"
    "  nop                               \n"
    "  str r1, [r0]                      \n"
    // forward point
    "  mov r0, r2                        \n"
    "  mov pc, lr                        \n"
    // return: always succeeds, old value
    );

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

asm (
    ".p2align(12)                        \n"
    ".global kern_lib_start              \n" // need this for mem_space.cpp
    "kern_lib_start:                     \n"

    // no restart through kernel entry code

    // atomic add
    // r0: memory reference
    // r1: delta value
    // r2: temp register
    // r3: temp register
    " 1:                                 \n"
    " ldrex r2, [r0]                     \n"
    " add   r2, r2, r1                   \n"
    " strex r3, r2, [r0]                 \n"
    " teq r3, #0                         \n"
    " bne 1b                             \n"
    " mov r0, r2                         \n"
    " mov pc, lr                         \n"
    // return: always succeeds, new value


    // compare exchange
    // r0: memory reference
    // r1: cmp value
    // r2: new value
    // r3: tmp reg
    ".p2align(8)                         \n"
    "  1: ldrex r3, [r0]		 \n"
    "  cmp r3, r1                        \n"
    "  movne r0, #0                      \n"
    "  movne pc, lr                      \n"
    "  strex r3, r2, [r0]                \n"
    "  teq r3, #0                        \n"
    "  bne 1b                            \n"
    "  mov r0, #1                        \n"
    "  mov pc, lr                        \n"
    // return result: 1 success, 0 failure


    // exchange
    //  in-r0: memory reference
    //  in-r1: new value
    // out-r0: old value
    ".p2align(8)                         \n"
    "  1:                                \n"
    "  ldrex r2, [r0]			 \n"
    "  strex r3, r1, [r0]                \n"
    "  cmp r3, #0                        \n"
    "  bne 1b                            \n"
    "  mov r0, r2                        \n"
    "  mov pc, lr                        \n"
    // return: always succeeds, old value
    );
