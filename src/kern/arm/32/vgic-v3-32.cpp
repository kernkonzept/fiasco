IMPLEMENTATION  [cpu_virt && vgic]:

PUBLIC static inline Gic_h::Hcr
Gic_h::hcr()
{
  Unsigned32 v;
  asm volatile ("mrc p15, 4, %0, c12, c11, 0" : "=r"(v));
  return Hcr(v);
}

PUBLIC static inline void
Gic_h::hcr(Gic_h::Hcr hcr)
{
  asm volatile ("mcr p15, 4, %0, c12, c11, 0" : : "r"(hcr.raw));
}

PUBLIC static inline Gic_h::Vtr
Gic_h::vtr()
{
  Unsigned32 v;
  asm volatile ("mrc p15, 4, %0, c12, c11, 1" : "=r"(v));
  return Vtr(v);
}

PUBLIC static inline Gic_h::Vmcr
Gic_h::vmcr()
{
  Unsigned32 v;
  asm volatile ("mrc p15, 4, %0, c12, c11, 7" : "=r"(v));
  return Vmcr(v);
}

PUBLIC static inline void
Gic_h::vmcr(Gic_h::Vmcr vmcr)
{
  asm volatile ("mcr p15, 4, %0, c12, c11, 7" : : "r"(vmcr.raw));
}

PUBLIC static inline Gic_h::Misr
Gic_h::misr()
{
  Unsigned32 v;
  asm volatile ("mrc p15, 4, %0, c12, c11, 2" : "=r"(v));
  return Misr(v);
}

PUBLIC static inline Unsigned32
Gic_h::eisr(unsigned n)
{
  (void)n; // n must be 0
  Unsigned32 v;
  asm volatile ("mrc p15, 4, %0, c12, c11, 3" : "=r"(v));
  return v;
}

PUBLIC static inline Unsigned32
Gic_h::elsr(unsigned n)
{
  (void)n; // n must be 0
  Unsigned32 v;
  asm volatile ("mrc p15, 4, %0, c12, c11, 5" : "=r"(v));
  return v;
}

PUBLIC static inline void
Gic_h::save_aprs(Gic_h::Aprs *a)
{
  // NOTE: we should use ASM patching to do this and just
  // replace instructions with NOPs
  asm ("mrc p15, 4, %0, c12, c8, 0" : "=r"(a->ap0r[0]));
  if (n_aprs > 1)
    asm ("mrc p15, 4, %0, c12, c8, 1" : "=r"(a->ap0r[1]));
  if (n_aprs > 2)
    {
      asm ("mrc p15, 4, %0, c12, c8, 2" : "=r"(a->ap0r[2]));
      asm ("mrc p15, 4, %0, c12, c8, 3" : "=r"(a->ap0r[3]));
    }

  asm ("mrc p15, 4, %0, c12, c9, 0" : "=r"(a->ap1r[0]));
  if (n_aprs > 1)
    asm ("mrc p15, 4, %0, c12, c9, 1" : "=r"(a->ap1r[1]));
  if (n_aprs > 2)
    {
      asm ("mrc p15, 4, %0, c12, c9, 2" : "=r"(a->ap1r[2]));
      asm ("mrc p15, 4, %0, c12, c9, 3" : "=r"(a->ap1r[3]));
    }
}

PUBLIC static inline void
Gic_h::load_aprs(Gic_h::Aprs const *a)
{
  // NOTE: we should use ASM patching to do this and just
  // replace instructions with NOPs
  asm ("mcr p15, 4, %0, c12, c8, 0" : : "r"(a->ap0r[0]));
  if (n_aprs > 1)
    asm ("mcr p15, 4, %0, c12, c8, 1" : : "r"(a->ap0r[1]));
  if (n_aprs > 2)
    {
      asm ("mcr p15, 4, %0, c12, c8, 2" : : "r"(a->ap0r[2]));
      asm ("mcr p15, 4, %0, c12, c8, 3" : : "r"(a->ap0r[3]));
    }

  asm ("mcr p15, 4, %0, c12, c9, 0" : : "r"(a->ap1r[0]));
  if (n_aprs > 1)
    asm ("mcr p15, 4, %0, c12, c9, 1" : : "r"(a->ap1r[1]));
  if (n_aprs > 2)
    {
      asm ("mcr p15, 4, %0, c12, c9, 2" : : "r"(a->ap1r[2]));
      asm ("mcr p15, 4, %0, c12, c9, 3" : : "r"(a->ap1r[3]));
    }
}

PUBLIC static inline void ALWAYS_INLINE
Gic_h::save_lrs(Gic_h::Lr *lr, unsigned n)
{
  Unsigned32 l, h;

#define READ_LR(ul,uh,v,x) \
  asm ("mrc p15, 4, %0, c12, " #ul", " #v : "=r"(l)); \
  asm ("mrc p15, 4, %0, c12, " #uh", " #v : "=r"(h)); \
  lr[x].raw = (((Unsigned64)h) << 32) | l; \
  if (n <= x + 1) return

  READ_LR(c12, c14, 0, 0);
  READ_LR(c12, c14, 1, 1);
  READ_LR(c12, c14, 2, 2);
  READ_LR(c12, c14, 3, 3);
  READ_LR(c12, c14, 4, 4);
  READ_LR(c12, c14, 5, 5);
  READ_LR(c12, c14, 6, 6);
  READ_LR(c12, c14, 7, 7);
  READ_LR(c13, c15, 0, 8);
  READ_LR(c13, c15, 1, 9);
  READ_LR(c13, c15, 2, 10);
  READ_LR(c13, c15, 3, 11);
  READ_LR(c13, c15, 4, 12);
  READ_LR(c13, c15, 5, 13);
  READ_LR(c13, c15, 6, 14);
  READ_LR(c13, c15, 7, 15);
#undef READ_LR
}

PUBLIC static inline void ALWAYS_INLINE
Gic_h::load_lrs(Gic_h::Lr const *lr, unsigned n)
{
#define READ_LR(ul,uh,v,x) \
  asm ("mcr p15, 4, %0, c12, " #ul", " #v : : "r"((Unsigned32)lr[x].raw)); \
  asm ("mcr p15, 4, %0, c12, " #uh", " #v : : "r"((Unsigned32)(lr[x].raw >> 32))); \
  if (n <= x + 1) return

  READ_LR(c12, c14, 0, 0);
  READ_LR(c12, c14, 1, 1);
  READ_LR(c12, c14, 2, 2);
  READ_LR(c12, c14, 3, 3);
  READ_LR(c12, c14, 4, 4);
  READ_LR(c12, c14, 5, 5);
  READ_LR(c12, c14, 6, 6);
  READ_LR(c12, c14, 7, 7);
  READ_LR(c13, c15, 0, 8);
  READ_LR(c13, c15, 1, 9);
  READ_LR(c13, c15, 2, 10);
  READ_LR(c13, c15, 3, 11);
  READ_LR(c13, c15, 4, 12);
  READ_LR(c13, c15, 5, 13);
  READ_LR(c13, c15, 6, 14);
  READ_LR(c13, c15, 7, 15);
#undef READ_LR
}

