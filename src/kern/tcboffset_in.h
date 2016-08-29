  DUMP_MEMBER1 (THREAD, Context, _state,		STATE)
  DUMP_MEMBER1 (THREAD, Context, _kernel_sp,		KERNEL_SP)
  DUMP_MEMBER1 (THREAD, Context, _donatee,		DONATEE)
  DUMP_MEMBER1 (THREAD, Context, _lock_cnt,		LOCK_CNT)
  DUMP_MEMBER1 (THREAD, Context, _sched_context,	SCHED_CONTEXT)
  DUMP_MEMBER1 (THREAD, Context, _sched,		SCHED)
  DUMP_MEMBER1 (THREAD, Context, _fpu_state,		FPU_STATE)
  DUMP_MEMBER1 (THREAD, Context, _consumed_time,	CONSUMED_TIME)
  DUMP_MEMBER1 (THREAD, Thread, _caller,		REPLY_CAP)
  DUMP_MEMBER1 (THREAD, Receiver, _partner,		PARTNER)
  DUMP_MEMBER1 (THREAD, Receiver, _rcv_regs,		RCV_REGS)
  DUMP_MEMBER1 (THREAD, Thread, _timeout,		TIMEOUT)
  DUMP_MEMBER1 (THREAD, Thread, _space,		SPACE)
  DUMP_MEMBER1 (THREAD, Thread, _pager,			PAGER)
  DUMP_MEMBER1 (THREAD, Thread, _recover_jmpbuf,	RECOVER_JMPBUF)
#if defined(CONFIG_ARM) && defined(CONFIG_BIT32)
  DUMP_MEMBER1 (THREAD, Thread, _exc_cont._ip,          EXCEPTION_IP)
  DUMP_MEMBER1 (THREAD, Thread, _exc_cont._psr,         EXCEPTION_PSR)
#endif
  DUMP_MEMBER1 (THREAD, Thread, _magic,			MAGIC)
  DUMP_OFFSET  (THREAD, MAX, sizeof (Thread))

  DUMP_MEMBER1 (THREAD, Context, _vcpu_state._u,        USER_VCPU)
  DUMP_MEMBER1 (THREAD, Context, _vcpu_state._k,        KERN_VCPU)
#if defined(CONFIG_ARM)
  DUMP_OFFSET  (THREAD, UTCB_SIZE, sizeof(Utcb))
#else
  DUMP_MEMBER1 (THREAD, Context, _vcpu_state._k,	VCPU_STATE)
#endif
#if 0
  DUMP_MEMBER1 (SCHED_CONTEXT, Sched_context,_owner,		OWNER)
  DUMP_MEMBER1 (SCHED_CONTEXT, Sched_context,_id,		ID)
  DUMP_MEMBER1 (SCHED_CONTEXT, Sched_context,_prio,		PRIO)
  DUMP_MEMBER1 (SCHED_CONTEXT, Sched_context,_quantum,		QUANTUM)
  DUMP_MEMBER1 (SCHED_CONTEXT, Sched_context,_left	,	LEFT)
  DUMP_MEMBER1 (SCHED_CONTEXT, Sched_context,_preemption_time,	PREEMPTION_TIME)
  DUMP_MEMBER1 (SCHED_CONTEXT, Sched_context,_prev,		PREV)
  DUMP_MEMBER1 (SCHED_CONTEXT, Sched_context,_next,		NEXT)
#endif
  DUMP_OFFSET  (SCHED_CONTEXT, MAX, sizeof (Sched_context))

  DUMP_MEMBER1 (MEM_SPACE, Mem_space, _dir,                     PGTABLE)


  DUMP_MEMBER1 (TBUF_STATUS, Tracebuffer_status, kerncnts,      KERNCNTS)

  DUMP_CAST_OFFSET (Thread, Receiver)
  DUMP_CAST_OFFSET (Thread, Sender)

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
  DUMP_THREAD_STATE (Thread_ipc_mask)
  DUMP_THREAD_STATE (Thread_ext_vcpu_enabled)
  DUMP_THREAD_STATE (Thread_fpu_owner)
  DUMP_CONSTANT (Thread_alien_or_vcpu_user, Thread_vcpu_user | Thread_alien)
  DUMP_CONSTANT (Thread_vcpu_state_mask, Thread_vcpu_state_mask)

#ifdef CONFIG_IA32
  DUMP_CONSTANT (MEM_LAYOUT__PHYSMEM,          Mem_layout::Physmem)
  DUMP_CONSTANT (MEM_LAYOUT__TBUF_STATUS_PAGE, Mem_layout::Tbuf_status_page)
#endif
#ifdef CONFIG_PF_PC
  DUMP_CONSTANT (MEM_LAYOUT__LAPIC,            Mem_layout::Local_apic_page)
  DUMP_CONSTANT (MEM_LAYOUT__IO_BITMAP,        Mem_layout::Io_bitmap)
  DUMP_CONSTANT (MEM_LAYOUT__SYSCALLS,         Mem_layout::Syscalls)
#endif
#ifdef CONFIG_PF_UX
  DUMP_CONSTANT (MEM_LAYOUT__TRAMPOLINE_PAGE,  Mem_layout::Trampoline_page)
#endif
#if defined(CONFIG_IA32) || defined(CONFIG_AMD64)
  DUMP_MEMBER1 (CPU, Cpu, tss, TSS)
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
