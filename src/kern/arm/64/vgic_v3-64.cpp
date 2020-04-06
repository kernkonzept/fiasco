IMPLEMENTATION  [cpu_virt && vgic]:

PUBLIC static inline Gic_h::Hcr
Gic_h_v3::hcr()
{
  Unsigned32 v;
  asm volatile ("mrs %x0, S3_4_C12_C11_0" : "=r"(v));
  return Hcr(v);
}

PUBLIC static inline void
Gic_h_v3::hcr(Gic_h::Hcr hcr)
{
  asm volatile ("msr S3_4_C12_C11_0, %x0" : : "r"(hcr.raw));
}

PUBLIC static inline Gic_h::Vtr
Gic_h_v3::vtr()
{
  Unsigned32 v;
  asm volatile ("mrs %x0, S3_4_C12_C11_1" : "=r"(v));
  return Vtr(v);
}

PUBLIC static inline Gic_h::Vmcr
Gic_h_v3::vmcr()
{
  Unsigned32 v;
  asm volatile ("mrs %x0, S3_4_C12_C11_7" : "=r"(v));
  return Vmcr(v);
}

PUBLIC static inline void
Gic_h_v3::vmcr(Gic_h::Vmcr vmcr)
{
  asm volatile ("msr S3_4_C12_C11_7, %x0" : : "r"(vmcr.raw));
}

PUBLIC static inline Gic_h::Misr
Gic_h_v3::misr()
{
  Unsigned32 v;
  asm volatile ("mrs %x0, S3_4_C12_C11_2" : "=r"(v));
  return Misr(v);
}

PUBLIC static inline Unsigned32
Gic_h_v3::eisr()
{
  Unsigned32 v;
  asm volatile ("mrs %x0, S3_4_C12_C11_3" : "=r"(v));
  return v;
}

PUBLIC static inline Unsigned32
Gic_h_v3::elsr()
{
  Unsigned32 v;
  asm volatile ("mrs %x0, S3_4_C12_C11_5" : "=r"(v));
  return v;
}


PUBLIC inline void
Gic_h_v3::save_aprs(Unsigned32 *a)
{
  // NOTE: we should use ASM patching to do this and just
  // replace instructions with NOPs
#define READ_APR(x) do { \
  asm ("mrs %x0, S3_4_C12_C8_" #x : "=r"(a[x * 2])); \
  asm ("mrs %x0, S3_4_C12_C9_" #x : "=r"(a[x * 2 + 1])); } while (0)

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
  asm ("msr S3_4_C12_C8_" #x ", %x0" : : "r"(a[x * 2])); \
  asm ("msr S3_4_C12_C9_" #x ", %x0" : : "r"(a[x * 2 + 1])); } while(0)

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

PUBLIC static inline ALWAYS_INLINE void
Gic_h_v3::save_lrs(Gic_h::Arm_vgic::Lrs *lr, unsigned n)
{
#define TRANSFER_LR(ul,v,x) \
  asm ("mrs %x0, S3_4_C12_" #ul "_" #v : "=r"(lr->lr64[x])); \
  if (n <= x + 1) return

  TRANSFER_LR(c12, 0, 0);
  TRANSFER_LR(c12, 1, 1);
  TRANSFER_LR(c12, 2, 2);
  TRANSFER_LR(c12, 3, 3);
  TRANSFER_LR(c12, 4, 4);
  TRANSFER_LR(c12, 5, 5);
  TRANSFER_LR(c12, 6, 6);
  TRANSFER_LR(c12, 7, 7);
  TRANSFER_LR(c13, 0, 8);
  TRANSFER_LR(c13, 1, 9);
  TRANSFER_LR(c13, 2, 10);
  TRANSFER_LR(c13, 3, 11);
  TRANSFER_LR(c13, 4, 12);
  TRANSFER_LR(c13, 5, 13);
  TRANSFER_LR(c13, 6, 14);
  TRANSFER_LR(c13, 7, 15);
#undef TRANSFER_LR
}

PUBLIC static inline ALWAYS_INLINE void
Gic_h_v3::load_lrs(Gic_h::Arm_vgic::Lrs const *lr, unsigned n)
{
#define TRANSFER_LR(ul,v,x) \
  asm ("msr S3_4_C12_" #ul "_" #v ", %x0" : : "r"(lr->lr64[x])); \
  if (n <= x + 1) return

  TRANSFER_LR(c12, 0, 0);
  TRANSFER_LR(c12, 1, 1);
  TRANSFER_LR(c12, 2, 2);
  TRANSFER_LR(c12, 3, 3);
  TRANSFER_LR(c12, 4, 4);
  TRANSFER_LR(c12, 5, 5);
  TRANSFER_LR(c12, 6, 6);
  TRANSFER_LR(c12, 7, 7);
  TRANSFER_LR(c13, 0, 8);
  TRANSFER_LR(c13, 1, 9);
  TRANSFER_LR(c13, 2, 10);
  TRANSFER_LR(c13, 3, 11);
  TRANSFER_LR(c13, 4, 12);
  TRANSFER_LR(c13, 5, 13);
  TRANSFER_LR(c13, 6, 14);
  TRANSFER_LR(c13, 7, 15);
#undef TRANSFER_LR
}
