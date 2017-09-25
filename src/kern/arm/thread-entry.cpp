INTERFACE:

IMPLEMENTATION[entry]:

#include <cstdio>
#include "kdb_ke.h"

extern "C" void kernel_entry(Mword pc, Mword)
{
  Mword pfa;
  asm volatile (" mrc p15, 0, %0, c6, c0 \n" : "=r"(pfa) );
  printf("PF: @%08x PC=%08x\n",pfa,pc);

  kdb_ke("page fault");
};
