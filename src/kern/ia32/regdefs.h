#ifndef REGDEFS_H
#define REGDEFS_H

//
// Register and register bit definitions for ia32, amd64 and UX architectures.
//

#define CR0_PE          0x00000001      // Protection Enable
#define CR0_MP          0x00000002      // Monitor Coprocessor
#define CR0_EM          0x00000004      // Emulation Coprocessor
#define CR0_TS          0x00000008      // Task Switched
#define CR0_ET          0x00000010      // Extension Type
#define CR0_NE          0x00000020      // Numeric Error
#define CR0_WP          0x00010000      // Write Protect
#define CR0_AM          0x00040000      // Alignment Mask
#define CR0_NW          0x20000000      // Not Write-Through
#define CR0_CD          0x40000000      // Cache Disable
#define CR0_PG          0x80000000      // Paging

#define CR3_PWT         0x00000008      // Page-Level Write Transparent
#define CR3_PCD         0x00000010      // Page-Level Cache Disable

#define CR4_VME         0x00000001      // Virtual 8086 Mode Extensions
#define CR4_PVI         0x00000002      // Protected Mode Virtual Ints
#define CR4_TSD         0x00000004      // Time Stamp Disable
#define CR4_DE          0x00000008      // Debugging Extensions
#define CR4_PSE         0x00000010      // Page Size Extensions
#define CR4_PAE         0x00000020      // Physical Address Extensions
#define CR4_MCE         0x00000040      // Machine Check Exception
#define CR4_PGE         0x00000080      // Page Global Enable
#define CR4_PCE         0x00000100      // Perfmon Counter Enable
#define CR4_OSFXSR      0x00000200      // OS Supports FXSAVE/FXRSTOR
#define CR4_OSXMMEXCPT  0x00000400      // OS Supports SIMD Exceptions
#define CR4_VMXE        0x00002000      // VMX enable
#define CR4_OSXSAVE     0x00040000      // OS Support XSAVE
#define CR4_SMEP        0x00100000      // Supervisor-Mode Execution Prevention

#define EFLAGS_CF       0x00000001      // Carry Flag
#define EFLAGS_PF       0x00000004      // Parity Flag
#define EFLAGS_AF       0x00000010      // Adjust Flag
#define EFLAGS_ZF       0x00000040      // Zero Flag
#define EFLAGS_SF       0x00000080      // Sign Flag
#define EFLAGS_TF       0x00000100      // Trap Flag
#define EFLAGS_IF       0x00000200      // Interrupt Enable
#define EFLAGS_DF       0x00000400      // Direction Flag
#define EFLAGS_OF       0x00000800      // Overflow Flag
#define EFLAGS_IOPL     0x00003000      // I/O Privilege Level (12+13)
#define EFLAGS_IOPL_K   0x00000000                      // kernel
#define EFLAGS_IOPL_U   0x00003000                      // user
#define EFLAGS_NT       0x00004000      // Nested Task
#define EFLAGS_RF       0x00010000      // Resume
#define EFLAGS_VM       0x00020000      // Virtual 8086 Mode
#define EFLAGS_AC       0x00040000      // Alignment Check
#define EFLAGS_VIF      0x00080000      // Virtual Interrupt
#define EFLAGS_VIP      0x00100000      // Virtual Interrupt Pending
#define EFLAGS_ID       0x00200000      // Identification

// CPU Feature Flags
#define FEAT_FPU        0x00000001      // FPU On Chip
#define FEAT_VME        0x00000002      // Virt. 8086 Mode Enhancements
#define FEAT_DE         0x00000004      // Debugging Extensions
#define FEAT_PSE        0x00000008      // Page Size Extension
#define FEAT_TSC        0x00000010      // Time Stamp Counter
#define FEAT_MSR        0x00000020      // Model Specific Registers
#define FEAT_PAE        0x00000040      // Physical Address Extension
#define FEAT_MCE        0x00000080      // Machine Check Exception
#define FEAT_CX8        0x00000100      // CMPXCHG8B Instruction
#define FEAT_APIC       0x00000200      // APIC On Chip
#define FEAT_SEP        0x00000800      // Sysenter/Sysexit Present
#define FEAT_MTRR       0x00001000      // Memory Type Range Registers
#define FEAT_PGE        0x00002000      // PTE Global Bit Extension
#define FEAT_MCA        0x00004000      // Machine Check Architecture
#define FEAT_CMOV       0x00008000      // Conditional Move Instruction
#define FEAT_PAT        0x00010000      // Page Attribute Table
#define FEAT_PSE36      0x00020000      // 32 bit Page Size Extension
#define FEAT_PSN        0x00040000      // Processor Serial Number
#define FEAT_CLFSH      0x00080000      // CLFLUSH Instruction
#define FEAT_DS         0x00200000      // Debug Store
#define FEAT_ACPI       0x00400000      // Thermal Monitor & Clock
#define FEAT_MMX        0x00800000      // MMX Technology
#define FEAT_FXSR       0x01000000      // FXSAVE/FXRSTOR Instructions
#define FEAT_SSE        0x02000000      // SSE
#define FEAT_SSE2       0x04000000      // SSE2
#define FEAT_SS         0x08000000      // Self Snoop
#define FEAT_HTT        0x10000000      // Hyper-Threading Technology
#define FEAT_TM         0x20000000      // Thermal Monitor
#define FEAT_PBE        0x80000000      // Pending Break Enable

// CPU Extended Feature Flags (Intel)
#define FEATX_SSE3      0x00000001      // SSE3
#define FEATX_MONITOR   0x00000008      // MONITOR/MWAIT Support
#define FEATX_DSCPL     0x00000010      // CPL Qualified Debug Store
#define FEATX_VMX       0x00000020      // Virtual Machine Extensions
#define FEATX_EST       0x00000080      // Enhanced SpeedStep Technology
#define FEATX_TM2       0x00000100      // Thermal Monitor 2
#define FEATX_SSSE3     0x00000200      // SSSE3
#define FEATX_CID       0x00000400      // L1 Context ID (adaptive/shared)
#define FEATX_CX16      0x00002000      // CMPXCHG16B Instruction
#define FEATX_XTPR      0x00004000      // Disable Task Priority Messages
#define FEATX_PCID      0x00020000      // PCID
#define FEATX_SSE4_1    0x00080000      // SSE4_1
#define FEATX_SSE4_2    0x00100000      // SSE4_2
#define FEATX_X2APIC    0x00200000      // x2APIC
#define FEATX_AES       0x02000000      // AES instructions
#define FEATX_XSAVE     0x04000000      // XSAVE
#define FEATX_OSXSAVE   0x08000000      // OSXSAVE
#define FEATX_AVX       0x10000000      // AVX

#define FEATX_IA32_TSC_ADJUST 0x00000002 // IA32 TSC Adjust available
#define FEATX_SMEP      0x00000080      // Supervisor-Mode Execution Prevention

// AMD: CPU Feature Flags, Fn80000001_ECX
#define FEATA_SVM	0x00000004

// AMD: CPU Feature Flags, Fn80000001_EDX
#define FEATA_SYSCALL   0x00000800      // Syscall/Sysret Present
#define FEATA_MP        0x00080000      // MP Capable
#define FEATA_NX        0x00100000      // No-Execute Page Protection
#define FEATA_MMXEXT    0x00400000      // AMD Extensions to MMX
#define FEATA_LM        0x20000000      // Long Mode
#define FEATA_3DNOWEXT  0x40000000      // AMD 3DNow! extensions
#define FEATA_3DNOW     0x80000000      // 3DNow!

// Page Fault Error Codes
// PF_ERR_REMTADDR and PF_ERR_USERADDR are UX-emulation only
#define PF_ERR_PRESENT  0x00000001      // PF: Page Is Present In PTE
#define PF_ERR_WRITE    0x00000002      // PF: Page Is Write Protected
#define PF_ERR_USERMODE 0x00000004      // PF: Caused By User Mode Code
#define PF_ERR_RESERVED 0x00000008      // PF: Reserved Bit Set in PDIR
#define PF_ERR_REMTADDR 0x40000000      // PF: In Remote Address Space
#define PF_ERR_USERADDR 0x80000000      // PF: In User Address Space

// Model Specific Registers
#define MSR_TSC              0x010      // Time Stamp Counter
#define MSR_IA32_FEATURE_CONTROL 0x03a  // Control Features in Intel 64 Processor
#define MSR_IA32_TSC_ADJUST  0x03b      // TSC adjust value
#define MSR_SYSENTER_CS      0x174      // Kernel Code Segment
#define MSR_SYSENTER_ESP     0x175      // Kernel Syscall Entry
#define MSR_SYSENTER_EIP     0x176      // Kernel Stack Pointer
#define MSR_LER_FROM_LIP     0x1d7      // Last Exception Record From Linear
#define MSR_LER_TO_LIP       0x1d8      // Last Exception Record To Linear
#define MSR_DEBUGCTLA        0x1d9      // Debug Control
#define MSR_LASTBRANCH_TOS   0x1da      // (P4) Last Branch Record Stack TOS
#define MSR_LASTBRANCH_0     0x1db      // (P4) Last Branch Record 0
#define MSR_LASTBRANCH_1     0x1dc      // (P4) Last Branch Record 1
#define MSR_LASTBRANCH_2     0x1dd      // (P4) Last Branch Record 2
#define MSR_LASTBRANCH_3     0x1de      // (P4) Last Branch Record 3
#define MSR_LASTBRANCHFROMIP 0x1db      // (P6)
#define MSR_LASTBRANCHTOIP   0x1dc      // (P6)
#define MSR_LASTINTFROMIP    0x1dd      // (P6)
#define MSR_LASTINTTOIP      0x1de      // (P6)
#define MSR_PAT              0x277      // PAT
#define MSR_HWP_PM_ENABLE    0x770      // HWP enable/disable
#define MSR_HWP_CAPABILITIES 0x771
#define MSR_HWP_REQUEST_PKG  0x772
#define MSR_HWP_INTERRUPT    0x773
#define MSR_HWP_REQUEST      0x774
#define MSR_HWP_STATUS       0x775

// AMD64 Model Specific Registers
#define MSR_EFER        0xc0000080      // Extended Feature Enable Register
#define MSR_STAR        0xc0000081      // CS and SS for Syscall/Sysret
#define MSR_LSTAR       0xc0000082      // EIP for Syscall (64Bit-Mode)
#define MSR_CSTAR       0xc0000083      // EIP for Syscall (Comp-Mode)
#define MSR_SFMASK      0xc0000084      // RFLAGS for Syscall
#define MSR_FS_BASE     0xc0000100      // FS-Base
#define MSR_GS_BASE     0xc0000101      // GS-Base
#define MSR_KERNEL_GS_BASE 0xc0000102   // Kernel-GS-Base
#define MSR_VM_CR       0xc0010114      // SVM
#define MSR_VM_HSAVE_PA 0xc0010117      // SVM host state-save area

#endif
