IMPLEMENTATION [arm]:

PRIVATE static void
Mem_op::inv_icache(Address start, Address end)
{
  if (Address(end) - Address(start) > 0x2000)
    asm volatile("mcr p15, 0, r0, c7, c5, 0");
  else
    {
      Mword s = Mem_unit::icache_line_size();
      for (start &= ~(s - 1);
           start < end; start += s)
        asm volatile("mcr p15, 0, %0, c7, c5, 1" : : "r" (start));
    }
}

