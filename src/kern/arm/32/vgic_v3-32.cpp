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

PUBLIC static inline ALWAYS_INLINE void
Gic_h_v3::read_apr(unsigned opc2, Unsigned32 *a)
{
  asm volatile ("mrc p15, 4, %0, c12, c8, %c1" : "=r"(a[0]) : "i"(opc2));
  asm volatile ("mrc p15, 4, %0, c12, c9, %c1" : "=r"(a[1]) : "i"(opc2));
}

PUBLIC static inline ALWAYS_INLINE void
Gic_h_v3::write_apr(unsigned opc2, Unsigned32 r0, Unsigned32 r1)
{
  asm volatile ("mcr p15, 4, %0, c12, c8, %c1" : : "r"(r0), "i"(opc2));
  asm volatile ("mcr p15, 4, %0, c12, c9, %c1" : : "r"(r1), "i"(opc2));
}

PUBLIC inline void
Gic_h_v3::save_aprs(Unsigned32 *a)
{
  // NOTE: ASM patching and replace instructions with NOPs
  read_apr(0, a + 0);
  if (n_aprs > 1)
    read_apr(1, a + 2);
  if (n_aprs > 2)
    {
      read_apr(2, a + 4);
      read_apr(3, a + 6);
    }
}

PUBLIC inline void
Gic_h_v3::load_aprs(Unsigned32 const *a)
{
  // NOTE: ASM patching and replace instructions with NOPs
  write_apr(0, a[0], a[1]);
  if (n_aprs > 1)
    write_apr(1, a[2], a[3]);
  if (n_aprs > 2)
    {
      write_apr(2, a[4], a[5]);
      write_apr(3, a[6], a[7]);
    }
}

PUBLIC static inline ALWAYS_INLINE void
Gic_h_v3::read_lr(Unsigned64 *a, unsigned crm, unsigned opc2)
{
  Unsigned32 l, h;
  asm volatile ("mrc p15, 4, %0, c12, c%c1, %c2" : "=r"(l) : "i"(crm), "i"(opc2));
  asm volatile ("mrc p15, 4, %0, c12, c%c1, %c2" : "=r"(h) : "i"(crm + 2), "i"(opc2));
  *a = (Unsigned64{h} << 32) | l;
}

PUBLIC static inline ALWAYS_INLINE void
Gic_h_v3::write_lr(Unsigned64 a, unsigned crm, unsigned opc2)
{
  asm volatile ("mcr p15, 4, %0, c12, c%c1, %c2" :: "r"(a), "i"(crm), "i"(opc2));
  asm volatile ("mcr p15, 4, %0, c12, c%c1, %c2" :: "r"(a >> 32), "i"(crm + 2), "i"(opc2));
}

PUBLIC static inline ALWAYS_INLINE void
Gic_h_v3::save_lrs(Gic_h::Arm_vgic::Lrs *lr)
{
#define TRANSFER_LR(i) \
  read_lr(lr->lr64 + i, 12 + i/8, i % 8); \
  if (Gic_h::Arm_vgic::N_lregs <= i + 1) return
  TRANSFER_LR(0);
  TRANSFER_LR(1);
  TRANSFER_LR(2);
  TRANSFER_LR(3);
  TRANSFER_LR(4);
  TRANSFER_LR(5);
  TRANSFER_LR(6);
  TRANSFER_LR(7);
  TRANSFER_LR(8);
  TRANSFER_LR(9);
  TRANSFER_LR(10);
  TRANSFER_LR(11);
  TRANSFER_LR(12);
  TRANSFER_LR(13);
  TRANSFER_LR(14);
  TRANSFER_LR(15);
#undef TRANSFER_LR
}

PUBLIC static inline ALWAYS_INLINE void
Gic_h_v3::load_lrs(Gic_h::Arm_vgic::Lrs const *lr)
{
#define TRANSFER_LR(i) \
  write_lr(lr->lr64[i], 12 + i/8, i % 8); \
  if (Gic_h::Arm_vgic::N_lregs <= i + 1) return
  TRANSFER_LR(0);
  TRANSFER_LR(1);
  TRANSFER_LR(2);
  TRANSFER_LR(3);
  TRANSFER_LR(4);
  TRANSFER_LR(5);
  TRANSFER_LR(6);
  TRANSFER_LR(7);
  TRANSFER_LR(8);
  TRANSFER_LR(9);
  TRANSFER_LR(10);
  TRANSFER_LR(11);
  TRANSFER_LR(12);
  TRANSFER_LR(13);
  TRANSFER_LR(14);
  TRANSFER_LR(15);
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

#define TRANSFER_LR(i) \
  if constexpr (i < Gic_h::Arm_vgic::N_lregs) \
    write_lr(new_lr.raw, 12 + i/8, i % 8)

  if (load)
    switch (idx)
      {
        case  0: TRANSFER_LR(0); break;
        case  1: TRANSFER_LR(1); break;
        case  2: TRANSFER_LR(2); break;
        case  3: TRANSFER_LR(3); break;
        case  4: TRANSFER_LR(4); break;
        case  5: TRANSFER_LR(5); break;
        case  6: TRANSFER_LR(6); break;
        case  7: TRANSFER_LR(7); break;
        case  8: TRANSFER_LR(8); break;
        case  9: TRANSFER_LR(9); break;
        case 10: TRANSFER_LR(10); break;
        case 11: TRANSFER_LR(11); break;
        case 12: TRANSFER_LR(12); break;
        case 13: TRANSFER_LR(13); break;
        case 14: TRANSFER_LR(14); break;
        case 15: TRANSFER_LR(15); break;
      }

#undef TRANSFER_LR
}

PUBLIC inline void
Gic_h_v3::clear_lr(unsigned idx)
{
#define TRANSFER_LR(i) \
  if constexpr (i < Gic_h::Arm_vgic::N_lregs) \
    asm volatile ("mcr p15, 4, %0, c12, c%c1, %c2" :: "r"(0), "i"(12 + i/8), "i"(i % 8))

  switch (idx)
    {
      case  0: TRANSFER_LR(0); break;
      case  1: TRANSFER_LR(1); break;
      case  2: TRANSFER_LR(2); break;
      case  3: TRANSFER_LR(3); break;
      case  4: TRANSFER_LR(4); break;
      case  5: TRANSFER_LR(5); break;
      case  6: TRANSFER_LR(6); break;
      case  7: TRANSFER_LR(7); break;
      case  8: TRANSFER_LR(8); break;
      case  9: TRANSFER_LR(9); break;
      case 10: TRANSFER_LR(10); break;
      case 11: TRANSFER_LR(11); break;
      case 12: TRANSFER_LR(12); break;
      case 13: TRANSFER_LR(13); break;
      case 14: TRANSFER_LR(14); break;
      case 15: TRANSFER_LR(15); break;
    }

#undef TRANSFER_LR
}

PUBLIC inline bool
Gic_h_v3::teardown_lr(Gic_h::Arm_vgic::Lrs *lr, unsigned idx)
{
  Lr reg(lr->lr64[idx]);
  if (!reg.active())
    {
      lr->lr64[idx] = 0;
      return false;
    }
  else
    return true;
}
