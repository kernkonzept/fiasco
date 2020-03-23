IMPLEMENTATION  [cpu_virt && vgic]:

PUBLIC static inline Gic_h::Hcr
Gic_h_v3::hcr()
{
  Unsigned32 v;
  asm volatile ("mrs %0, S3_4_C12_C11_0" : "=r"(v));
  return Hcr(v);
}

PUBLIC static inline void
Gic_h_v3::hcr(Gic_h::Hcr hcr)
{
  asm volatile ("msr S3_4_C12_C11_0, %0" : : "r"(hcr.raw));
}

PUBLIC static inline Gic_h::Vtr
Gic_h_v3::vtr()
{
  Unsigned32 v;
  asm volatile ("mrs %0, S3_4_C12_C11_1" : "=r"(v));
  return Vtr(v);
}

PUBLIC static inline Gic_h::Vmcr
Gic_h_v3::vmcr()
{
  Unsigned32 v;
  asm volatile ("mrs %0, S3_4_C12_C11_7" : "=r"(v));
  return Vmcr(v);
}

PUBLIC static inline void
Gic_h_v3::vmcr(Gic_h::Vmcr vmcr)
{
  asm volatile ("msr S3_4_C12_C11_7, %0" : : "r"(vmcr.raw));
}

PUBLIC static inline Gic_h::Misr
Gic_h_v3::misr()
{
  Unsigned32 v;
  asm volatile ("mrs %0, S3_4_C12_C11_2" : "=r"(v));
  return Misr(v);
}

PUBLIC static inline Unsigned32
Gic_h_v3::eisr()
{
  Unsigned32 v;
  asm volatile ("mrs %0, S3_4_C12_C11_3" : "=r"(v));
  return v;
}

PUBLIC static inline Unsigned32
Gic_h_v3::elsr()
{
  Unsigned32 v;
  asm volatile ("mrs %0, S3_4_C12_C11_5" : "=r"(v));
  return v;
}


PUBLIC inline void
Gic_h_v3::save_aprs(Unsigned32 *a)
{
  // NOTE: we should use ASM patching to do this and just
  // replace instructions with NOPs
#define READ_APR(x) do { \
  asm ("mrs %0, S3_4_C12_C8_" #x : "=r"(a[x * 2])); \
  asm ("mrs %0, S3_4_C12_C9_" #x : "=r"(a[x * 2 + 1])); } while (0)

  READ_APR(0);
  if (n_aprs > 1)
    READ_APR(1);
  if (n_aprs > 2)
    {
      READ_APR(2);
      READ_APR(3);
    }
#undef READ_APR
}

PUBLIC inline void
Gic_h_v3::load_aprs(Unsigned32 const *a)
{
  // NOTE: we should use ASM patching to do this and just
  // replace instructions with NOPs
#define READ_APR(x) do { \
  asm ("msr S3_4_C12_C8_" #x ", %0" : : "r"(a[x * 2])); \
  asm ("msr S3_4_C12_C9_" #x ", %0" : : "r"(a[x * 2 + 1])); } while(0)

  READ_APR(0);
  if (n_aprs > 1)
    READ_APR(1);
  if (n_aprs > 2)
    {
      READ_APR(2);
      READ_APR(3);
    }
#undef READ_APR
}

PUBLIC static inline void ALWAYS_INLINE
Gic_h_v3::save_lrs(Gic_h::Arm_vgic::Lrs *lr, unsigned n)
{
#define READ_LR(ul,v,x) \
  asm ("mrs %0, S3_4_C12_" #ul "_" #v : "=r"(lr->lr64[x])); \
  if (n <= x + 1) return

  READ_LR(c12, 0, 0);
  READ_LR(c12, 1, 1);
  READ_LR(c12, 2, 2);
  READ_LR(c12, 3, 3);
  READ_LR(c12, 4, 4);
  READ_LR(c12, 5, 5);
  READ_LR(c12, 6, 6);
  READ_LR(c12, 7, 7);
  READ_LR(c13, 0, 8);
  READ_LR(c13, 1, 9);
  READ_LR(c13, 2, 10);
  READ_LR(c13, 3, 11);
  READ_LR(c13, 4, 12);
  READ_LR(c13, 5, 13);
  READ_LR(c13, 6, 14);
  READ_LR(c13, 7, 15);
#undef READ_LR
}

PUBLIC static inline void ALWAYS_INLINE
Gic_h_v3::load_lrs(Gic_h::Arm_vgic::Lrs const *lr, unsigned n)
{
#define READ_LR(ul,v,x) \
  asm ("msr S3_4_C12_" #ul "_" #v ", %0" : : "r"(lr->lr64[x])); \
  if (n <= x + 1) return

  READ_LR(c12, 0, 0);
  READ_LR(c12, 1, 1);
  READ_LR(c12, 2, 2);
  READ_LR(c12, 3, 3);
  READ_LR(c12, 4, 4);
  READ_LR(c12, 5, 5);
  READ_LR(c12, 6, 6);
  READ_LR(c12, 7, 7);
  READ_LR(c13, 0, 8);
  READ_LR(c13, 1, 9);
  READ_LR(c13, 2, 10);
  READ_LR(c13, 3, 11);
  READ_LR(c13, 4, 12);
  READ_LR(c13, 5, 13);
  READ_LR(c13, 6, 14);
  READ_LR(c13, 7, 15);
#undef READ_LR
}
