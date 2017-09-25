IMPLEMENTATION [ia32]:

PRIVATE static inline FIASCO_INIT_CPU
void
Kmem::init_cpu_arch(Cpu &cpu, void **cpu_mem)
{
  // allocate the task segment for the double fault handler
  cpu.init_tss_dbf (__alloc(cpu_mem, sizeof(Tss)),
      Mem_layout::pmem_to_phys(Kmem::dir()));

  cpu.init_sysenter();
}

