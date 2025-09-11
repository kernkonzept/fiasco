  DUMP_MEMBER1 (THREAD, Context, _state,		STATE)
  DUMP_MEMBER1 (THREAD, Context, _lock_cnt,		LOCK_CNT)
#if defined(CONFIG_ARM) && defined(CONFIG_BIT32)
  DUMP_MEMBER1 (THREAD, Thread, _exc_cont._ip,          EXCEPTION_IP)
  DUMP_MEMBER1 (THREAD, Thread, _exc_cont._psr,         EXCEPTION_PSR)
#endif
#if defined(CONFIG_ARM) && defined(CONFIG_MPU)
  DUMP_MEMBER1 (THREAD, Context, _mpu_ku_mem_regions,   KU_MEM_REGIONS)
  DUMP_MEMBER1 (THREAD, Context, _mpu_prbar2,           MPU_PRBAR2)
  DUMP_MEMBER1 (THREAD, Context, _mpu_prlar2,           MPU_PRLAR2)
#endif

  DUMP_MEMBER1 (THREAD, Context, _vcpu_state._u,        USER_VCPU)
  DUMP_MEMBER1 (THREAD, Context, _vcpu_state._k,        KERN_VCPU)
#if defined(CONFIG_ARM)
  DUMP_OFFSET  (THREAD, UTCB_SIZE, sizeof(Utcb))
#else
  DUMP_MEMBER1 (THREAD, Context, _vcpu_state._k,	VCPU_STATE)
#endif

  DUMP_CONSTANT (SIZEOF_TRAP_STATE, sizeof(Trap_state))
#if defined(CONFIG_AMD64) || defined(CONFIG_IA32)
  DUMP_MEMBER1 (TRAP_STATE, Trap_state, _ip, IP)
#endif
  DUMP_MEMBER1 (VCPU_STATE, Vcpu_state, _regs, TREX)
  DUMP_MEMBER1 (VCPU_STATE, Vcpu_state, _entry_ip, ENTRY_IP)
  DUMP_MEMBER1 (VCPU_STATE, Vcpu_state, _sp, ENTRY_SP)

  DUMP_THREAD_STATE (Thread_ready)
  DUMP_THREAD_STATE (Thread_cancel)
  DUMP_THREAD_STATE (Thread_dis_alien)
#if defined (CONFIG_IA32) || defined (CONFIG_AMD64)
  DUMP_BITSHIFT (Thread_dis_alien, Thread_dis_alien)
#endif
  DUMP_THREAD_STATE (Thread_ipc_mask)
  DUMP_THREAD_STATE (Thread_ext_vcpu_enabled)
  DUMP_THREAD_STATE (Thread_fpu_owner)
  DUMP_CONSTANT (Thread_alien_or_vcpu_user, Thread_vcpu_user | Thread_alien)
  DUMP_CONSTANT (Thread_vcpu_state_mask, Thread_vcpu_state_mask)

#ifdef CONFIG_IA32
  DUMP_CONSTANT (MEM_LAYOUT__PHYSMEM,          Mem_layout::Physmem)
#endif
#ifdef CONFIG_PF_PC
# ifdef CONFIG_IA32
  DUMP_CONSTANT (MEM_LAYOUT__SYSCALLS,         Mem_layout::Syscalls)
# endif
# ifdef CONFIG_AMD64
  DUMP_CONSTANT (MEM_LAYOUT__KENTRY_CPU_PAGE,  Mem_layout::Kentry_cpu_page)
# endif
#endif
#if defined(CONFIG_IA32) || defined(CONFIG_AMD64)
  DUMP_MEMBER1 (CPU, Cpu, _tss, TSS)
#endif

#ifdef CONFIG_PPC32
  //offset of entry frame
  DUMP_OFFSET (ENTRY, FRAME, THREAD_BLOCK_SIZE-4)
  //offset of syscall frame 
  DUMP_OFFSET (SYSCALL, FRAME, sizeof(Return_frame))
  DUMP_CONSTANT(ENTRY__FRAME, (sizeof(Entry_frame)))
  //Entry_frame size + SysV LR (16 byte aligned)
  DUMP_CONSTANT(STACK__FRAME, ((sizeof(Entry_frame) + 8 + 0xf) & ~0xf))
  //SRR1 || MSR flags
  DUMP_CONSTANT(MSR__KERNEL, Msr::Msr_kernel)
  DUMP_CONSTANT(MSR__PR, Msr::Msr_pr)

  //physical atddress of kernel image
  DUMP_CONSTANT(KERNEL__START, Mem_layout::Kernel_start)
#endif
#ifdef  CONFIG_MIPS
  DUMP_MEMBER1 (THREAD, Context, _ulr, ULR)
#endif
