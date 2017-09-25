// Source for generating some asm for cut'n'paste, why program asm ourselves
// when the compiler can do it?
// arm-linux-g++ -fverbose-asm -fPIC -W -Wall -Os -S cache_func_gen.cpp

#include <cstdio>

enum {
  DEBUG = 0,
};

void num_way_l1_cache_op()
{
  asm volatile("@ Start:");
  register unsigned long ccsidr;
  asm volatile("mcr p15, 2, %0, c0, c0, 0" : : "r" (0)); // L1, data or unified
  asm volatile("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr));

  if (DEBUG)
    printf("ccsidr = %08lx\n", ccsidr);

  // sets
  register unsigned numsets = ((ccsidr >> 13) & ((1 << 15) - 1));
  // associativity - 1
  register unsigned numways = ((ccsidr >>  3) & ((1 << 10) - 1));
  // linesize
  register unsigned linesizel2 = (ccsidr >> 0) & ((1 << 3) - 1);

  if (DEBUG)
    printf("linesizel2: %d  numways: %d numsets: %d\n",
           linesizel2, numways + 1, numsets + 1);

  register int shiftways = __builtin_clz(numways);
  register int shiftset  = linesizel2 + 4;

  if (DEBUG)
    printf("shiftways: %d shiftset: %d\n", shiftways, shiftset);

  unsigned int cnt = 0;
  for (register int w = numways; w >= 0; --w)
    for (register int s = numsets; s >= 0; --s)
      {
        register unsigned long v = (w << shiftways) | (s << shiftset);
        if (!DEBUG)
          // invalidate num/way
          asm volatile("mcr p15, 0, %0, c7, c6, 2" : : "r" (v));
        if (DEBUG)
          printf("w=%d s=%d: %08lx\n", w, s, v);
        if (DEBUG)
          cnt++;
      }

  asm volatile("isb");
  asm volatile("dsb");
  asm volatile("@ End:");
  if (DEBUG)
    printf("cnt: %d\n", cnt);
}
