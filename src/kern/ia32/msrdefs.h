#pragma once

#include "types.h"

/**
 * Model Specific Registers.
 *
 * Registers with prefix "IA32_" ("Ia32_") in the list below) are considered
 * by Intel as "architectural MSRs" and do not change on future processors.
 */
enum class Msr : Unsigned32
{
  // ***********
  // Common MSRs
  // ***********
  Ia32_tsc = 0x010,                 // Time Stamp Counter
  Ia32_platform_id = 0x017,         // Platform ID
  Ia32_apic_base = 0x01b,           // APIC base address
  Ia32_feature_control = 0x03a,     // Control Features in Intel 64 Processor
  Ia32_tsc_adjust = 0x03b,          // TSC adjust value
  Ia32_spec_ctrl = 0x048,           // Speculation Control
  Ia32_pred_cmd = 0x049,            // Predication Command
  Ia32_bios_updt_trig = 0x079,      // BIOS Update Trigger Register
  Ia32_bios_sign_id = 0x08b,        // BIOS Update Signature ID
  Ia32_mtrrcap = 0x0fe,             // MTRR Capability
  Ia32_arch_capabilities = 0x10a,   // Enumeration of Architectural Features
  Ia32_flush_cmd = 0x10b,           // Flush L1D cache
  Ia32_sysenter_cs = 0x174,         // Kernel Code Segment
  Ia32_sysenter_esp = 0x175,        // Kernel Syscall Entry
  Ia32_sysenter_eip = 0x176,        // Kernel Stack Pointer
  Ia32_perf_ctl = 0x199,            // Performance Control
  Ia32_mtrr_phybase0 = 0x200,
  Ia32_mtrr_phybase1 = 0x201,
  Ia32_misc_enable = 0x1a0,         // Enable Misc. Processor Features
  Ler_from_lip = 0x1d7,             // Last Exception Record From Linear
  Ler_to_lip = 0x1d8,               // Last Exception Record To Linear
  Debugctla = 0x1d9,                // Debug Control
  Lastbranch_tos = 0x1da,           // (P4) Last Branch Record Stack TOS
  Lastbranch_0 = 0x1db,             // (P4) Last Branch Record 0
  Lastbranch_1 = 0x1dc,             // (P4) Last Branch Record 1
  Lastbranch_2 = 0x1dd,             // (P4) Last Branch Record 2
  Lastbranch_3 = 0x1de,             // (P4) Last Branch Record 3
  Lastbranchfromip = 0x1db,         // (P6)
  Lastbranchtoip = 0x1dc,           // (P6)
  Lastintfromip = 0x1dd,            // (P6)
  Lastinttoip = 0x1de,              // (P6)
  Ia32_pat = 0x277,                 // PAT
  Ia32_fixed_ctr_ctrl = 0x38d,      // Fixed-Function Performance Ctr Control
  Ia32_perf_global_ctrl = 0x38f,    // Global Performance Counter Control
  Ia32_ds_area = 0x600,             // DS Save Area
  Lastbranch_0_from_ip = 0x680,     // Last Branch Record 0 From IP
  Lastbranch_0_to_ip = 0x6c0,       // Last Branch Record 0 To IP
  Hwp_pm_enable = 0x770,            // HWP enable/disable
  Hwp_capabilities = 0x771,
  Hwp_request_pkg = 0x772,
  Hwp_interrupt = 0x773,
  Hwp_request = 0x774,
  Hwp_status = 0x775,
  X2apic_regs = 0x800,

  // ********************************
  // Performance Counter related MSRs
  // ********************************
  P5_cesr = 0x11,
  P5_ctr0 = 0x12,
  P5_ctr1 = 0x13,
  P6_perfctr0 = 0xC1,
  P6_evntsel0 = 0x186,
  K7_evntsel0 = 0xC0010000,
  K7_perfctr0 = 0xC0010004,
  P4_misc_enable = 0x1A0,
  P4_perfctr0 = 0x300,
  P4_bpu_counter0 = 0x300,
  P4_cccr0 = 0x360,
  P4_fsb_escr0 = 0x3A2,
  Ap_perfctr0 = 0xC1,
  Ap_evntsel0 = 0x186,
  Bsu_escr0 = 0x3a0,                // .. 0x3b9 (26)
  Iq_escr0 = 0x3ba,                 // .. 0x3bb (2)
  Rat_escr0 = 0x3bc,                // .. 0x3bd (2)
  Ssu_escr0 = 0x3be,                // .. 0x3be (1)
  Ms_escr0 = 0x3c0,                 // .. 0x3c1 (2)
  Tbpu_escr0 = 0x3c2,               // .. 0x3c3 (2)
  Tc_escr0 = 0x3c4,                 // .. 0x3c5 (2)
  Ix_escr0 = 0x3c8,                 // .. 0x3c9 (2)
  Cru_escr0 = 0x3ca,                // .. 0x3cd (4)
  Cru_escr4 = 0x3e0,                // .. 0x3e1 (2)
  Ia32_pebs_enable = 0x3f1,         // Processor Event Based Sampling
  Pebs_matrix_vert = 0x3f2,


  // **********
  // AMD64 MSRs
  // **********
  Ia32_efer = 0xc0000080,           // Extended Feature Enable Register
  Ia32_star = 0xc0000081,           // CS and SS for Syscall/Sysret
  Ia32_lstar = 0xc0000082,          // EIP for Syscall (64Bit-Mode)
  Ia32_cstar = 0xc0000083,          // EIP for Syscall (Comp-Mode)
  Ia32_fmask = 0xc0000084,          // RFLAGS for Syscall
  Ia32_fs_base = 0xc0000100,        // FS-Base
  Ia32_gs_base = 0xc0000101,        // GS-Base
  Ia32_kernel_gs_base = 0xc0000102, // Kernel-GS-Base
  Osvw_id_length = 0xc0010140,      // OS visible workaround length


  // **********************
  // Intel VMX-related MSRs
  // **********************
  // Reporting Register of Basic VMX
  Ia32_vmx_basic = 0x480,
  // Capability Reporting Register of Pin-Based VM-Execution
  Ia32_vmx_pinbased_ctls = 0x481,
  // Capability Reporting Register of Primary Processor-Based VM-Execution
  // Controls
  Ia32_vmx_procbased_ctls = 0x482,
  // Capability Reporting Register of VM-Exit Controls
  Ia32_vmx_exit_ctls = 0x483,
  // Capability Reporting Register of VM-Entry Controls
  Ia32_vmx_entry_ctls = 0x484,
  // Reporting Register of Miscellaneous VMX Capabilities
  Ia32_vmx_misc = 0x485,
  Ia32_vmx_cr0_fixed0 = 0x486,
  Ia32_vmx_cr0_fixed1 = 0x487,
  Ia32_vmx_cr4_fixed0 = 0x488,
  Ia32_vmx_cr4_fixed1 = 0x489,
  Ia32_vmx_vmcs_enum = 0x48a,
  Ia32_vmx_procbased_ctls2 = 0x48b,
  Ia32_vmx_ept_vpid_cap = 0x48c,
  Ia32_vmx_true_pinbased_ctls = 0x48d,
  Ia32_vmx_true_procbased_ctls = 0x48e,
  Ia32_vmx_true_exit_ctls = 0x48f,
  Ia32_vmx_true_entry_ctls = 0x490,


  // ******************
  // AMD-V-related MSRs
  // ******************
  Vm_cr = 0xc0010114,               // SVM
  Vm_hsave_pa = 0xc0010117,         // SVM host state-save area
};
