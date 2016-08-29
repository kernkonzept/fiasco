IMPLEMENTATION [arm]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  asm volatile("tlbi vmalle1" : : "r" (0) : "memory");
}

IMPLEMENT inline
void Mem_unit::dtlb_flush(void *va)
{
  asm volatile("tlbi vae1, %0"
               : : "r" ((((unsigned long)va) >> 12) & 0x00000ffffffffffful)
               : "memory");
}


