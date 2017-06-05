INTERFACE[arm && 32bit]:

EXTENSION class Proc
{
public:
  enum : unsigned
  {
    Status_mode_user      = 0x10,
    Status_mode_always_on = 0x110,
    Status_mode_vmm       = 0x10,
  };
};

//--------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

IMPLEMENT static inline
void Proc::cli()
{
  asm volatile("cpsid " ARM_CPS_INTERRUPT_FLAGS : : : "memory");
}

IMPLEMENT static inline ALWAYS_INLINE
void Proc::sti()
{
  asm volatile("cpsie " ARM_CPS_INTERRUPT_FLAGS : : : "memory");
}

IMPLEMENT static inline ALWAYS_INLINE
Proc::Status Proc::cli_save()
{
  Status prev;
  asm volatile("mrs %0, cpsr \n"
               "cpsid " ARM_CPS_INTERRUPT_FLAGS
               : "=r" (prev)
               :
               : "memory");
  return prev;
}

//--------------------------------------------------------------------
IMPLEMENTATION [arm]:

IMPLEMENT static inline
Mword Proc::program_counter()
{
  Mword pc;
  asm ("mov %0, pc" : "=r" (pc));
  return pc;
}

IMPLEMENT static inline
Proc::Status Proc::interrupts()
{
  Status ret;
  asm volatile("mrs %0, cpsr" : "=r" (ret));
  return !(ret & Sti_mask);
}

//----------------------------------------------------------------
IMPLEMENTATION[arm && mp]:

IMPLEMENT static inline
Cpu_phys_id Proc::cpu_id()
{
  unsigned mpidr;
  __asm__("mrc p15, 0, %0, c0, c0, 5": "=r" (mpidr));
  return Cpu_phys_id(mpidr & 0xffffff);
}

//----------------------------------------------------------------
IMPLEMENTATION [arm && !arm_v6plus]:

IMPLEMENT static inline
void Proc::cli()
{
  Mword v;
  asm volatile("mrs %0, cpsr    \n"
               "orr %0, %0, %1  \n"
               "msr cpsr_c, %0  \n"
               : "=r" (v)
               : "I" (Cli_mask)
               : "memory");
}

IMPLEMENT static inline ALWAYS_INLINE
void Proc::sti()
{
  Mword v;
  asm volatile("mrs %0, cpsr    \n"
               "bic %0, %0, %1  \n"
               "msr cpsr_c, %0  \n"
               : "=r" (v)
               : "I" (Sti_mask)
               : "memory");
}

IMPLEMENT static inline ALWAYS_INLINE
Proc::Status Proc::cli_save()
{
  Status ret;
  Mword v;
  asm volatile("mrs %0, cpsr    \n"
               "orr %1, %0, %2  \n"
               "msr cpsr_c, %1  \n"
               : "=r" (ret), "=r" (v)
               : "I" (Cli_mask)
               : "memory");
  return ret;
}

//----------------------------------------------------------------
IMPLEMENTATION[arm && arm_926]:

IMPLEMENT static inline
void Proc::pause()
{
}

IMPLEMENT static inline
void Proc::halt()
{
  Status f = cli_save();
  asm volatile("mov     r0, #0                                              \n\t"
               "mrc     p15, 0, r1, c1, c0, 0       @ Read control register \n\t"
               "mcr     p15, 0, r0, c7, c10, 4      @ Drain write buffer    \n\t"
               "bic     r2, r1, #1 << 12                                    \n\t"
               "mcr     p15, 0, r2, c1, c0, 0       @ Disable I cache       \n\t"
               "mcr     p15, 0, r0, c7, c0, 4       @ Wait for interrupt    \n\t"
               "mcr     15, 0, r1, c1, c0, 0        @ Restore ICache enable \n\t"
               :::"memory",
               "r0", "r1", "r2", "r3", "r4", "r5",
               "r6", "r7", "r8", "r9", "r10", "r11",
               "r12", "r13", "r14", "r15"
      );
  sti_restore(f);
}

//----------------------------------------------------------------
IMPLEMENTATION[arm && arm_1136]:

IMPLEMENT static inline
void Proc::pause()
{}

IMPLEMENT static inline
void Proc::halt()
{
  Status f = cli_save();
  asm volatile("mcr     p15, 0, r0, c7, c10, 4  @ DWB/DSB \n\t"
               "mcr     p15, 0, r0, c7, c0, 4   @ WFI \n\t");
  sti_restore(f);
}

//----------------------------------------------------------------
IMPLEMENTATION[arm && (arm_1176 || arm_mpcore)]:

IMPLEMENT static inline
void Proc::pause()
{}

IMPLEMENT static inline
void Proc::halt()
{
  Status f = cli_save();
  asm volatile("mcr     p15, 0, r0, c7, c10, 4  @ DWB/DSB \n\t"
               "wfi \n\t");
  sti_restore(f);
}


