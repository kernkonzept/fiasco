#pragma once

/**
 * Model Specific Registers.
 *
 * Registers with prefix "IA32_" ("Msr_ia32_") in the list below) are considered
 * by Intel as "architectural MSRs" and do not change on future processors.
 */
enum
{
  // ***********
  // Common MSRs
  // ***********
  Msr_ia32_tsc = 0x010,                 // Time Stamp Counter
  Msr_ia32_platform_id = 0x017,         // Platform ID
  Msr_ia32_apic_base = 0x01b,           // APIC base address
  Msr_ia32_feature_control = 0x03a,     // Control Features in Intel 64 Processor
  Msr_ia32_tsc_adjust = 0x03b,          // TSC adjust value
  Msr_ia32_spec_ctrl = 0x048,           // Speculation Control
  Msr_ia32_pred_cmd = 0x049,            // Predication Command
  Msr_ia32_bios_updt_trig = 0x079,      // BIOS Update Trigger Register
  Msr_ia32_bios_sign_id = 0x08b,        // BIOS Update Signature ID
  Msr_ia32_mtrrcap = 0x0fe,             // MTRR Capability
  Msr_ia32_arch_capabilities = 0x10a,   // Enumeration of Architectural Features
  Msr_ia32_flush_cmd = 0x10b,           // Flush L1D cache
  Msr_ia32_sysenter_cs = 0x174,         // Kernel Code Segment
  Msr_ia32_sysenter_esp = 0x175,        // Kernel Syscall Entry
  Msr_ia32_sysenter_eip = 0x176,        // Kernel Stack Pointer
  Msr_ia32_perf_ctl = 0x199,            // Performance Control
  Msr_ia32_mtrr_phybase0 = 0x200,
  Msr_ia32_mtrr_phybase1 = 0x201,
  Msr_ia32_misc_enable = 0x1a0,         // Enable Misc. Processor Features
  Msr_ler_from_lip = 0x1d7,             // Last Exception Record From Linear
  Msr_ler_to_lip = 0x1d8,               // Last Exception Record To Linear
  Msr_debugctla = 0x1d9,                // Debug Control
  Msr_lastbranch_tos = 0x1da,           // (P4) Last Branch Record Stack TOS
  Msr_lastbranch_0 = 0x1db,             // (P4) Last Branch Record 0
  Msr_lastbranch_1 = 0x1dc,             // (P4) Last Branch Record 1
  Msr_lastbranch_2 = 0x1dd,             // (P4) Last Branch Record 2
  Msr_lastbranch_3 = 0x1de,             // (P4) Last Branch Record 3
  Msr_lastbranchfromip = 0x1db,         // (P6)
  Msr_lastbranchtoip = 0x1dc,           // (P6)
  Msr_lastintfromip = 0x1dd,            // (P6)
  Msr_lastinttoip = 0x1de,              // (P6)
  Msr_ia32_pat = 0x277,                 // PAT
  Msr_ia32_fixed_ctr_ctrl = 0x38d,      // Fixed-Function Performance Ctr Ctrl
  Msr_ia32_perf_global_ctrl = 0x38f,    // Global Performance Counter Control
  Msr_ia32_ds_area = 0x600,             // DS Save Area
  Msr_lastbranch_0_from_ip = 0x680,     // Last Branch Record 0 From IP
  Msr_lastbranch_0_to_ip = 0x6c0,       // Last Branch Record 0 To IP
  Msr_hwp_pm_enable = 0x770,            // HWP enable/disable
  Msr_hwp_capabilities = 0x771,
  Msr_hwp_request_pkg = 0x772,
  Msr_hwp_interrupt = 0x773,
  Msr_hwp_request = 0x774,
  Msr_hwp_status = 0x775,
  // 0x800..0x83f: Local x2APIC

  // ********************************
  // Performance Counter related MSRs
  // ********************************
  Msr_p5_cesr = 0x11,
  Msr_p5_ctr0 = 0x12,
  Msr_p5_ctr1 = 0x13,
  Msr_p6_perfctr0 = 0xC1,
  Msr_p6_evntsel0 = 0x186,
  Msr_k7_evntsel0 = 0xC0010000,
  Msr_k7_perfctr0 = 0xC0010004,
  Msr_p4_misc_enable = 0x1A0,
  Msr_p4_perfctr0 = 0x300,
  Msr_p4_bpu_counter0 = 0x300,
  Msr_p4_cccr0 = 0x360,
  Msr_p4_fsb_escr0 = 0x3A2,
  Msr_ap_perfctr0 = 0xC1,
  Msr_ap_evntsel0 = 0x186,
  Msr_bsu_escr0 = 0x3a0,                // .. 0x3b9 (26)
  Msr_iq_escr0 = 0x3ba,                 // .. 0x3bb (2)
  Msr_rat_escr0 = 0x3bc,                // .. 0x3bd (2)
  Msr_ssu_escr0 = 0x3be,                // .. 0x3be (1)
  Msr_ms_escr0 = 0x3c0,                 // .. 0x3c1 (2)
  Msr_tbpu_escr0 = 0x3c2,               // .. 0x3c3 (2)
  Msr_tc_escr0 = 0x3c4,                 // .. 0x3c5 (2)
  Msr_ix_escr0 = 0x3c8,                 // .. 0x3c9 (2)
  Msr_cru_escr0 = 0x3ca,                // .. 0x3cd (4)
  Msr_cru_escr4 = 0x3e0,                // .. 0x3e1 (2)
  Msr_ia32_pebs_enable = 0x3f1,         // Processor Event Based Sampling
  Msr_pebs_matrix_vert = 0x3f2,


  // **********
  // AMD64 MSRs
  // **********
  Msr_ia32_efer = 0xc0000080,           // Extended Feature Enable Register
  Msr_ia32_star = 0xc0000081,           // CS and SS for Syscall/Sysret
  Msr_ia32_lstar = 0xc0000082,          // EIP for Syscall (64Bit-Mode)
  Msr_ia32_cstar = 0xc0000083,          // EIP for Syscall (Comp-Mode)
  Msr_ia32_fmask = 0xc0000084,          // RFLAGS for Syscall
  Msr_ia32_fs_base = 0xc0000100,        // FS-Base
  Msr_ia32_gs_base = 0xc0000101,        // GS-Base
  Msr_ia32_kernel_gs_base = 0xc0000102, // Kernel-GS-Base
  Msr_osvw_id_length = 0xc0010140,      // OS visible workaround length


  // **********************
  // Intel VMX-related MSRs
  // **********************
  // Reporting Register of Basic VMX
  Msr_ia32_vmx_basic = 0x480,
  // Capability Reporting Register of Pin-Based VM-Execution
  Msr_ia32_vmx_pinbased_ctls = 0x481,
  // Capability Reporting Register of Primary Processor-Based VM-Execution
  // Controls
  Msr_ia32_vmx_procbased_ctls = 0x482,
  // Capability Reporting Register of VM-Exit Controls
  Msr_ia32_vmx_exit_ctls = 0x483,
  // Capability Reporting Register of VM-Entry Controls
  Msr_ia32_vmx_entry_ctls = 0x484,
  // Reporting Register of Miscellaneous VMX Capabilities
  Msr_ia32_vmx_misc = 0x485,
  Msr_ia32_vmx_cr0_fixed0 = 0x486,
  Msr_ia32_vmx_cr0_fixed1 = 0x487,
  Msr_ia32_vmx_cr4_fixed0 = 0x488,
  Msr_ia32_vmx_cr4_fixed1 = 0x489,
  Msr_ia32_vmx_vmcs_enum = 0x48a,
  Msr_ia32_vmx_procbased_ctls2 = 0x48b,
  Msr_ia32_vmx_ept_vpid_cap = 0x48c,
  Msr_ia32_vmx_true_pinbased_ctls = 0x48d,
  Msr_ia32_vmx_true_procbased_ctls = 0x48e,
  Msr_ia32_vmx_true_exit_ctls = 0x48f,
  Msr_ia32_vmx_true_entry_ctls = 0x490,


  // ******************
  // AMD-V-related MSRs
  // ******************
  Msr_vm_cr = 0xc0010114,               // SVM
  Msr_vm_hsave_pa = 0xc0010117,         // SVM host state-save area
};
