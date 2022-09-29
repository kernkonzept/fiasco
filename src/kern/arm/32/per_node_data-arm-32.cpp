IMPLEMENTATION [amp && cpu_virt]:

IMPLEMENT static inline
char *
Per_node_data_base::local_addr(char *node0_obj)
{
  unsigned long offset;
  asm("mrc p15, 4, %0, c13, c0, 2": "=r"(offset)); // HTPIDR
  return node0_obj + offset;
}

// --------------------------------------------------------------------------
IMPLEMENTATION [amp && !cpu_virt]:

IMPLEMENT static inline
char *
Per_node_data_base::local_addr(char *node0_obj)
{
  unsigned long offset;
  asm("mrc p15, 0, %0, c13, c0, 4": "=r"(offset)); // TPIDRPRW
  return node0_obj + offset;
}
