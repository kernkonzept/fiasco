IMPLEMENTATION  [cpu_virt && vgic]:

PUBLIC static inline Gic_h::Hcr
Gic_h_v3::hcr()
{
  Unsigned32 v;
  asm volatile ("mrc p15, 4, %0, c12, c11, 0" : "=r"(v));
  return Hcr(v);
}

PUBLIC static inline void
Gic_h_v3::hcr(Gic_h::Hcr hcr)
{
  asm volatile ("mcr p15, 4, %0, c12, c11, 0" : : "r"(hcr.raw));
}

PUBLIC static inline Gic_h::Vtr
Gic_h_v3::vtr()
{
  Unsigned32 v;
  asm volatile ("mrc p15, 4, %0, c12, c11, 1" : "=r"(v));
  return Vtr(v);
}

PUBLIC static inline Gic_h::Vmcr
Gic_h_v3::vmcr()
{
  Unsigned32 v;
  asm volatile ("mrc p15, 4, %0, c12, c11, 7" : "=r"(v));
  return Vmcr(v);
}

PUBLIC static inline void
Gic_h_v3::vmcr(Gic_h::Vmcr vmcr)
{
  asm volatile ("mcr p15, 4, %0, c12, c11, 7" : : "r"(vmcr.raw));
}

PUBLIC static inline Gic_h::Misr
Gic_h_v3::misr()
{
  Unsigned32 v;
  asm volatile ("mrc p15, 4, %0, c12, c11, 2" : "=r"(v));
  return Misr(v);
}

PUBLIC static inline Unsigned32
Gic_h_v3::eisr()
{
  Unsigned32 v;
  asm volatile ("mrc p15, 4, %0, c12, c11, 3" : "=r"(v));
  return v;
}

PUBLIC static inline Unsigned32
Gic_h_v3::elsr()
{
  Unsigned32 v;
  asm volatile ("mrc p15, 4, %0, c12, c11, 5" : "=r"(v));
  return v;
}

PUBLIC inline void
Gic_h_v3::save_aprs(Unsigned32 *a)
{
  // NOTE: we should use ASM patching to do this and just
  // replace instructions with NOPs
  asm ("mrc p15, 4, %0, c12, c8, 0" : "=r"(a[0]));
  asm ("mrc p15, 4, %0, c12, c9, 0" : "=r"(a[1]));
  if (n_aprs > 1)
    {
      asm ("mrc p15, 4, %0, c12, c8, 1" : "=r"(a[2]));
      asm ("mrc p15, 4, %0, c12, c9, 1" : "=r"(a[3]));
    }
  if (n_aprs > 2)
    {
      asm ("mrc p15, 4, %0, c12, c8, 2" : "=r"(a[4]));
      asm ("mrc p15, 4, %0, c12, c9, 2" : "=r"(a[5]));
      asm ("mrc p15, 4, %0, c12, c8, 3" : "=r"(a[6]));
      asm ("mrc p15, 4, %0, c12, c9, 3" : "=r"(a[7]));
    }
}

PUBLIC inline void
Gic_h_v3::load_aprs(Unsigned32 const *a)
{
  // NOTE: we should use ASM patching to do this and just
  // replace instructions with NOPs
  asm ("mcr p15, 4, %0, c12, c8, 0" : : "r"(a[0]));
  asm ("mcr p15, 4, %0, c12, c9, 0" : : "r"(a[1]));
  if (n_aprs > 1)
    {
      asm ("mcr p15, 4, %0, c12, c8, 1" : : "r"(a[2]));
      asm ("mcr p15, 4, %0, c12, c9, 1" : : "r"(a[3]));
    }
  if (n_aprs > 2)
    {
      asm ("mcr p15, 4, %0, c12, c8, 2" : : "r"(a[4]));
      asm ("mcr p15, 4, %0, c12, c9, 2" : : "r"(a[5]));
      asm ("mcr p15, 4, %0, c12, c8, 3" : : "r"(a[6]));
      asm ("mcr p15, 4, %0, c12, c9, 3" : : "r"(a[7]));
    }
}

PUBLIC static inline ALWAYS_INLINE void
Gic_h_v3::save_lrs(Gic_h::Arm_vgic::Lrs *lr, unsigned n)
{
  Unsigned32 l, h;

#define TRANSFER_LR(ul,uh,v,x) \
  asm ("mrc p15, 4, %0, c12, " #ul", " #v : "=r"(l)); \
  asm ("mrc p15, 4, %0, c12, " #uh", " #v : "=r"(h)); \
  lr->lr64[x] = (((Unsigned64)h) << 32) | l; \
  if ((x + 1 >= Gic_h::Arm_vgic::N_lregs) || (n <= x + 1)) return

  TRANSFER_LR(c12, c14, 0, 0);
  TRANSFER_LR(c12, c14, 1, 1);
  TRANSFER_LR(c12, c14, 2, 2);
  TRANSFER_LR(c12, c14, 3, 3);
  TRANSFER_LR(c12, c14, 4, 4);
  TRANSFER_LR(c12, c14, 5, 5);
  TRANSFER_LR(c12, c14, 6, 6);
  TRANSFER_LR(c12, c14, 7, 7);
  TRANSFER_LR(c13, c15, 0, 8);
  TRANSFER_LR(c13, c15, 1, 9);
  TRANSFER_LR(c13, c15, 2, 10);
  TRANSFER_LR(c13, c15, 3, 11);
  TRANSFER_LR(c13, c15, 4, 12);
  TRANSFER_LR(c13, c15, 5, 13);
  TRANSFER_LR(c13, c15, 6, 14);
  TRANSFER_LR(c13, c15, 7, 15);
#undef TRANSFER_LR
}

PUBLIC static inline ALWAYS_INLINE void
Gic_h_v3::load_lrs(Gic_h::Arm_vgic::Lrs const *lr, unsigned n)
{
#define TRANSFER_LR(ul,uh,v,x) \
  asm ("mcr p15, 4, %0, c12, " #ul", " #v : : "r"((Unsigned32)lr->lr64[x])); \
  asm ("mcr p15, 4, %0, c12, " #uh", " #v : : "r"((Unsigned32)(lr->lr64[x] >> 32))); \
  if ((x + 1 >= Gic_h::Arm_vgic::N_lregs) || (n <= x + 1)) return

  TRANSFER_LR(c12, c14, 0, 0);
  TRANSFER_LR(c12, c14, 1, 1);
  TRANSFER_LR(c12, c14, 2, 2);
  TRANSFER_LR(c12, c14, 3, 3);
  TRANSFER_LR(c12, c14, 4, 4);
  TRANSFER_LR(c12, c14, 5, 5);
  TRANSFER_LR(c12, c14, 6, 6);
  TRANSFER_LR(c12, c14, 7, 7);
  TRANSFER_LR(c13, c15, 0, 8);
  TRANSFER_LR(c13, c15, 1, 9);
  TRANSFER_LR(c13, c15, 2, 10);
  TRANSFER_LR(c13, c15, 3, 11);
  TRANSFER_LR(c13, c15, 4, 12);
  TRANSFER_LR(c13, c15, 5, 13);
  TRANSFER_LR(c13, c15, 6, 14);
  TRANSFER_LR(c13, c15, 7, 15);
#undef TRANSFER_LR
}

PUBLIC inline void
Gic_h_v3::build_lr(Gic_h::Arm_vgic::Lrs *lr, unsigned idx,
                   Gic_h::Vcpu_irq_cfg cfg, bool load)
{
  Lr new_lr(0);
  new_lr.state() = Lr::Pending;
  new_lr.eoi()   = 1; // need an EOI IRQ
  new_lr.vid()   = cfg.vid();
  new_lr.prio()  = cfg.prio();
  new_lr.grp1()  = cfg.grp1();

  lr->lr64[idx] = new_lr.raw;

#define TRANSFER_LR(ul,uh,v,x) \
  if (x < Gic_h::Arm_vgic::N_lregs) \
    { \
      asm ("mcr p15, 4, %0, c12, " #ul", " #v : : "r"((Unsigned32)new_lr.raw)); \
      asm ("mcr p15, 4, %0, c12, " #uh", " #v : : "r"((Unsigned32)(new_lr.raw >> 32))); \
    }

  if (load)
    switch (idx)
      {
        case  0: TRANSFER_LR(c12, c14, 0, 0); break;
        case  1: TRANSFER_LR(c12, c14, 1, 1); break;
        case  2: TRANSFER_LR(c12, c14, 2, 2); break;
        case  3: TRANSFER_LR(c12, c14, 3, 3); break;
        case  4: TRANSFER_LR(c12, c14, 4, 4); break;
        case  5: TRANSFER_LR(c12, c14, 5, 5); break;
        case  6: TRANSFER_LR(c12, c14, 6, 6); break;
        case  7: TRANSFER_LR(c12, c14, 7, 7); break;
        case  8: TRANSFER_LR(c13, c15, 0, 8); break;
        case  9: TRANSFER_LR(c13, c15, 1, 9); break;
        case 10: TRANSFER_LR(c13, c15, 2, 10); break;
        case 11: TRANSFER_LR(c13, c15, 3, 11); break;
        case 12: TRANSFER_LR(c13, c15, 4, 12); break;
        case 13: TRANSFER_LR(c13, c15, 5, 13); break;
        case 14: TRANSFER_LR(c13, c15, 6, 14); break;
        case 15: TRANSFER_LR(c13, c15, 7, 15); break;
      }

#undef TRANSFER_LR
}

PUBLIC inline void
Gic_h_v3::clear_lr(unsigned idx)
{
#define TRANSFER_LR(uh,v,x) \
  if (x < Gic_h::Arm_vgic::N_lregs) \
    asm ("mcr p15, 4, %0, c12, " #uh", " #v : : "r"(0));

  switch (idx)
    {
      case  0: TRANSFER_LR(c14, 0, 0); break;
      case  1: TRANSFER_LR(c14, 1, 1); break;
      case  2: TRANSFER_LR(c14, 2, 2); break;
      case  3: TRANSFER_LR(c14, 3, 3); break;
      case  4: TRANSFER_LR(c14, 4, 4); break;
      case  5: TRANSFER_LR(c14, 5, 5); break;
      case  6: TRANSFER_LR(c14, 6, 6); break;
      case  7: TRANSFER_LR(c14, 7, 7); break;
      case  8: TRANSFER_LR(c15, 0, 8); break;
      case  9: TRANSFER_LR(c15, 1, 9); break;
      case 10: TRANSFER_LR(c15, 2, 10); break;
      case 11: TRANSFER_LR(c15, 3, 11); break;
      case 12: TRANSFER_LR(c15, 4, 12); break;
      case 13: TRANSFER_LR(c15, 5, 13); break;
      case 14: TRANSFER_LR(c15, 6, 14); break;
      case 15: TRANSFER_LR(c15, 7, 15); break;
    }

#undef TRANSFER_LR
}

PUBLIC inline bool
Gic_h_v3::teardown_lr(Gic_h::Arm_vgic::Lrs *lr, unsigned idx, bool reap)
{
  Lr reg(lr->lr64[idx]);
  if (!reg.active() || reap)
    {
      lr->lr64[idx] = 0;
      return false;
    }
  else
    return true;
}
