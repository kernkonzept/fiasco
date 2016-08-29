IMPLEMENTATION [arm]:

PRIVATE static void
Mem_op::inv_icache(Address start, Address end)
{
  if (Address(end) - Address(start) > 0x2000)
    asm volatile("ic iallu");
  else
    {
      Mword s = Mem_unit::icache_line_size();
      for (start &= ~(s - 1);
           start < end; start += s)
        asm volatile("ic ivau, %0" : : "r" (start));
    }
}


