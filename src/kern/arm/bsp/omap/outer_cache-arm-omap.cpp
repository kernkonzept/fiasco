IMPLEMENTATION [arm && pf_omap4 && arm_cache_l2cxx0]:

EXTENSION class Outer_cache
{
  enum
  {
    Omap_l2cache_set_debug_reg             = 0x100,
    Omap_l2cache_clean_and_inv_range       = 0x101,
    Omap_l2cache_enable                    = 0x102,
    Omap_l2cache_aux_reg                   = 0x109,
    Omap_l2cache_tag_and_data_ram_lat_ctrl = 0x112,
    Omap_l2cache_prefetch_ctrl             = 0x113,
  };
};

PRIVATE static inline
void
Outer_cache::smc(Mword func, Mword val)
{
  register Mword _func asm("r12") = func;
  register Mword _val  asm("r0")  = val;
  asm volatile("dsb; smc #0"
               :
               : "r" (_func), "r" (_val)
               : "memory", "cc", "r1", "r2", "r3", "r4", "r5",
                 "r6", "r7", "r8", "r9", "r10", "r11");
}

IMPLEMENT
Mword
Outer_cache::platform_init(Mword aux_control)
{
  aux_control =   (1 << 16) // 16-way associativity
                | (3 << 17) // 64k waysize
                | (1 << 22) // shared attrib override
                | (1 << 25) // reserved
                | (1 << 26) // ns lockdown enable
                | (1 << 27) // ns irq access enable
                | (1 << 28) // data prefetch
                | (1 << 29) // insn prefetch
                | (1 << 30) // early BRESP enable
               ;

  smc(Omap_l2cache_aux_reg, aux_control);
  smc(Omap_l2cache_enable, 1);

  return aux_control;
}
