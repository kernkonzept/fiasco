INTERFACE:

#include <cstdio>
#include <flux/x86/cpuid.h>

#define panic(fmt...) printf(fmt), exit(1)

#define check(expression) \
         ((void)((expression) ? 0 : \
                (panic(__FILE__":%u: failed check `"#expression"'", \
                        __LINE__), 0)))

class Thread;
class Context;
class Space;

extern cpu_info cpu;
extern Space* sigma0;

IMPLEMENTATION:

Space* sigma0 = 0;

cpu_info cpu = { 3, 8, CPU_FAMILY_PENTIUM_PRO, CPU_TYPE_ORIGINAL, 0, 
                 CPUF_ON_CHIP_FPU | 
		 CPUF_4MB_PAGES |
		 CPUF_CMPXCHG8B |
		 CPUF_PAGE_GLOBAL_EXT,
		 {'G', 'e', 'n', 'u', 'i', 'n', 'e', 'I', 'n', 't', 'e', 'l'}
               };

Context* current()
{
  return reinterpret_cast <Context*>(1);
}


