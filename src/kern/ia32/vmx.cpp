INTERFACE [vmx]:

#include "per_cpu_data.h"
#include "pm.h"

#include <cassert>
#include <cstdio>
#include <cstring>

class Vmx_info
{
public:

  template<typename T>
  class Bit_defs
  {
  protected:
    T _or;
    T _and;

    void enforce_bits(T m, bool value = true)
    {
      if (value)
        _or |= m;
      else
        _and &= ~m;
    }

    bool allowed_bits(T m, bool value = true) const
    {
      if (value)
        return _and & m;
      else
        return !(_or & m);
    }

  public:
    Bit_defs() {}
    Bit_defs(T _or, T _and) : _or(_or), _and(_and) {}

    T must_be_one() const { return _or; }
    T may_be_one() const { return _and; }

    T apply(T v) const { return (v | _or) & _and; }

    void print(char const *name) const
    {
      if (sizeof(T) <= 4)
        printf("%20s = %8x %8x\n", name, static_cast<Unsigned32>(_and),
               static_cast<Unsigned32>(_or));
      else if (sizeof(T) <= 8)
        printf("%20s = %16llx %16llx\n", name, Unsigned64{_and}, Unsigned64{_or});
    }
  };

  template<typename WORD_TYPE, typename BITS_TYPE>
  struct Bit_defs_t : Bit_defs<WORD_TYPE>
  {
    Bit_defs_t() = default;
    Bit_defs_t(WORD_TYPE _and, WORD_TYPE _or)
    : Bit_defs<WORD_TYPE>(_and, _or) {}

    Bit_defs_t(Bit_defs<WORD_TYPE> const &o) : Bit_defs<WORD_TYPE>(o) {}

    void relax(BITS_TYPE bit)
    {
      this->_or &= ~(WORD_TYPE{1} << static_cast<WORD_TYPE>(bit));
      this->_and |= WORD_TYPE{1} << static_cast<WORD_TYPE>(bit);
    }

    void enforce(BITS_TYPE bit, bool value = true)
    { this->enforce_bits(WORD_TYPE{1} << static_cast<WORD_TYPE>(bit), value); }

    bool allowed(BITS_TYPE bit, bool value = true) const
    {
      return this->allowed_bits(WORD_TYPE{1} << static_cast<WORD_TYPE>(bit),
                                value);
    }
  };

  template<typename BITS_TYPE>
  class Bit_defs_32 : public Bit_defs_t<Unsigned32, BITS_TYPE>
  {
  public:
    Bit_defs_32() {}
    Bit_defs_32(Unsigned64 v)
    : Bit_defs_t<Unsigned32, BITS_TYPE>(v, v >> 32)
    {}
  };

  typedef Bit_defs<Unsigned64> Bit_defs_64;

  template<typename T>
  class Flags
  {
  public:
    Flags() {}
    explicit Flags(T v) : _f(v) {}

    T test(unsigned char bit) const
    { return _f & (T{1} << T{bit}); }
  private:
    T _f;
  };

  enum Pin_based_ctls
  {
    PIB_ext_int_exit     = 0,
    PIB_nmi_exit         = 3,
    PIB_preemption_timer = 6,
  };

  enum Primary_proc_based_ctls
  {
    PRB1_rdpmc_exiting            = 11,
    PRB1_tpr_shadow               = 21,
    PRB1_mov_dr_exit              = 23,
    PRB1_unconditional_io_exit    = 24,
    PRB1_use_io_bitmaps           = 25,
    PRB1_use_msr_bitmaps          = 28,
    PRB1_enable_proc_based_ctls_2 = 31,
  };

  enum Secondary_proc_based_ctls
  {
    PRB2_virtualize_apic = 0,
    PRB2_enable_ept      = 1,
    PRB2_enable_vpid     = 5,
    PRB2_unrestricted    = 7,
    PRB2_enable_pml      = 17,
    PRB2_enable_xsaves   = 20,
    PRB2_tsc_scaling     = 25,
  };

  enum Entry_ctls
  {
    En_load_debug_ctls       = 2,
    En_ia32e_mode_guest      = 9,
    En_entry_to_smm          = 10,
    En_no_dual_monitor       = 11,
    En_load_perf_global_ctl  = 13,
    En_load_ia32_pat         = 14,
    En_load_ia32_efer        = 15,
  };

  enum Exit_ctls
  {
    Ex_save_debug_ctls        = 2,
    Ex_host_addr_size         = 9,
    Ex_load_perf_global_ctl   = 12,
    Ex_ack_irq_on_exit        = 15,
    Ex_save_ia32_pat          = 18,
    Ex_load_ia32_pat          = 19,
    Ex_save_ia32_efer         = 20,
    Ex_load_ia32_efer         = 21,
    Ex_save_preemption_timer  = 22,
  };

  enum Exceptions
  {
    Exception_db                 = 1,
    Exception_ac                 = 17,
  };

  Unsigned64 basic;

  Bit_defs_32<Pin_based_ctls> pinbased_ctls;
  Bit_defs_32<Primary_proc_based_ctls> procbased_ctls;

  Bit_defs_32<Exit_ctls> exit_ctls;
  Bit_defs_32<Entry_ctls> entry_ctls;
  Unsigned64 misc;

  Bit_defs_t<Mword, unsigned char> cr0_defs;
  Bit_defs_t<Mword, unsigned char> cr4_defs;
  Bit_defs_32<Secondary_proc_based_ctls> procbased_ctls2;
  Bit_defs_32<Exceptions> exception_bitmap;

  Unsigned64 ept_vpid_cap;

  bool has_invept() const { return ept_vpid_cap & (1 << 20); }
  bool has_invept_single() const { return ept_vpid_cap & (1 << 25); }
  bool has_invept_global() const { return ept_vpid_cap & (1 << 26); }

  Unsigned64 max_index;
  Unsigned32 pinbased_ctls_default1;
  Unsigned32 procbased_ctls_default1;
  Unsigned32 exit_ctls_default1;
  Unsigned32 entry_ctls_default1;
};

struct Vmx_user_info
{
  Unsigned64 basic;
  Vmx_info::Bit_defs_32<Vmx_info::Pin_based_ctls> pinbased;
  Vmx_info::Bit_defs_32<Vmx_info::Primary_proc_based_ctls> procbased;
  Vmx_info::Bit_defs_32<Vmx_info::Exit_ctls> exit;
  Vmx_info::Bit_defs_32<Vmx_info::Entry_ctls> entry;
  Unsigned64 misc;
  Unsigned64 cr0_or;
  Unsigned64 cr0_and;
  Unsigned64 cr4_or;
  Unsigned64 cr4_and;
  Unsigned64 vmcs_field_info;
  Vmx_info::Bit_defs_32<Vmx_info::Secondary_proc_based_ctls> procbased2;
  Unsigned64 ept_vpid_cap;
  Unsigned64 nested_revision;

  Unsigned32 pinbased_dfl1;
  Unsigned32 procbased_dfl1;
  Unsigned32 exit_dfl1;
  Unsigned32 entry_dfl1;
};

INTERFACE:

class Vmx : public Pm_object
{
public:
  /**
   * 16-bit guest VMCS fields supported by the implementation.
   */
  enum Vmcs_16bit_guest_fields : Unsigned16
  {
    Vmcs_guest_es_selector      = 0x0800,
    Vmcs_guest_cs_selector      = 0x0802,
    Vmcs_guest_ss_selector      = 0x0804,
    Vmcs_guest_ds_selector      = 0x0806,
    Vmcs_guest_fs_selector      = 0x0808,
    Vmcs_guest_gs_selector      = 0x080a,
    Vmcs_guest_ldtr_selector    = 0x080c,
    Vmcs_guest_tr_selector      = 0x080e,
    Vmcs_guest_interrupt_status = 0x0810,

    // Must be the last
    Max_16bit_guest
  };

  /**
   * 16-bit host VMCS fields managed by the implementation (not exposed to user
   * space).
   */
  enum Vmcs_16bit_host_fields : Unsigned16
  {
    Vmcs_host_es_selector  = 0x0c00,
    Vmcs_host_cs_selector  = 0x0c02,
    Vmcs_host_ss_selector  = 0x0c04,
    Vmcs_host_ds_selector  = 0x0c06,
    Vmcs_host_fs_selector  = 0x0c08,
    Vmcs_host_gs_selector  = 0x0c0a,
    Vmcs_host_tr_selector  = 0x0c0c,

    // Must be the last
    Max_16bit_host
  };

  /**
   * 64-bit control VMCS fields supported by the implementation.
   */
  enum Vmcs_64bit_ctl_fields : Unsigned16
  {
    Vmcs_tsc_offset          = 0x2010,
    Vmcs_apic_access_address = 0x2014,
    Vmcs_ept_pointer         = 0x201a,
    Vmcs_tsc_multiplier      = 0x2032,

    // Must be the last
    Max_64bit_ctl
  };

  /**
   * 64-bit read-only VMCS fields supported by the implementation.
   */
  enum Vmcs_64bit_ro_fields : Unsigned16
  {
    Vmcs_guest_physical_address = 0x2400,

    // Must be the last
    Max_64bit_ro
  };

  /**
   * 64-bit guest VMCS fields supported by the implementation.
   */
  enum Vmcs_64bit_guest_fields : Unsigned16
  {
    Vmcs_link_pointer                = 0x2800,
    Vmcs_guest_ia32_debugctl         = 0x2802,
    Vmcs_guest_ia32_pat              = 0x2804,
    Vmcs_guest_ia32_efer             = 0x2806,
    Vmcs_guest_ia32_perf_global_ctrl = 0x2808,

    // Must be the last
    Max_64bit_guest
  };

  /**
   * Software-defined 64-bit guest VMCS fields supported by the implementation.
   *
   * These fields are not present in the hardware VMCS, but represent
   * additional guest state exposed to user space.
   */
  enum Sw_64bit_guest_fields : Unsigned16
  {
    Sw_guest_xcr0         = 0x2880,
    Sw_msr_syscall_mask   = 0x2882,
    Sw_msr_lstar          = 0x2884,
    Sw_msr_cstar          = 0x2886,
    Sw_msr_tsc_aux        = 0x2888,
    Sw_msr_star           = 0x288a,
    Sw_msr_kernel_gs_base = 0x288c,

    // Must be the last
    Max_64bit_sw
  };

  /**
   * 64-bit host VMCS fields managed by the implementation (not exposed to user
   * space).
   */
  enum Vmcs_64bit_host_fields : Unsigned16
  {
    Vmcs_host_ia32_pat              = 0x2c00,
    Vmcs_host_ia32_efer             = 0x2c02,
    Vmcs_host_ia32_perf_global_ctrl = 0x2c04,

    // Must be the last
    Max_64bit_host
  };

  /**
   * 32-bit control VMCS fields supported by the implementation.
   */
  enum Vmcs_32bit_ctl_fields : Unsigned16
  {
    Vmcs_pin_based_vm_exec_ctls      = 0x4000,
    Vmcs_pri_proc_based_vm_exec_ctls = 0x4002,
    Vmcs_exception_bitmap            = 0x4004,
    Vmcs_page_fault_error_mask       = 0x4006,
    Vmcs_page_fault_error_match      = 0x4008,
    Vmcs_cr3_target_cnt              = 0x400a,
    Vmcs_vm_exit_ctls                = 0x400c,
    Vmcs_exit_msr_store_cnt          = 0x400e,
    Vmcs_exit_msr_load_cnt           = 0x4010,
    Vmcs_vm_entry_ctls               = 0x4012,
    Vmcs_entry_msr_load_cnt          = 0x4014,
    Vmcs_vm_entry_interrupt_info     = 0x4016,
    Vmcs_vm_entry_exception_error    = 0x4018,
    Vmcs_vm_entry_insn_len           = 0x401a,
    Vmcs_sec_proc_based_vm_exec_ctls = 0x401e,

    // Must be the last
    Max_32bit_ctl
  };

  /**
   * 32-bit read-only VMCS fields supported by the implementation.
   */
  enum Vmcs_32bit_ro_fields : Unsigned16
  {
    Vmcs_vm_insn_error           = 0x4400,
    Vmcs_exit_reason             = 0x4402,
    Vmcs_vm_exit_interrupt_info  = 0x4404,
    Vmcs_vm_exit_interrupt_error = 0x4406,
    Vmcs_idt_vectoring_info      = 0x4408,
    Vmcs_idt_vectoring_error     = 0x440a,
    Vmcs_vm_exit_insn_length     = 0x440c,
    Vmcs_vm_exit_insn_info       = 0x440e,

    // Must be the last
    Max_32bit_ro
  };

  /**
   * 32-bit guest VMCS fields supported by the implementation.
   */
  enum Vmcs_32bit_guest_fields : Unsigned16
  {
    Vmcs_guest_es_limit               = 0x4800,
    Vmcs_guest_cs_limit               = 0x4802,
    Vmcs_guest_ss_limit               = 0x4804,
    Vmcs_guest_ds_limit               = 0x4806,
    Vmcs_guest_fs_limit               = 0x4808,
    Vmcs_guest_gs_limit               = 0x480a,
    Vmcs_guest_ldtr_limit             = 0x480c,
    Vmcs_guest_tr_limit               = 0x480e,
    Vmcs_guest_gdtr_limit             = 0x4810,
    Vmcs_guest_idtr_limit             = 0x4812,
    Vmcs_guest_es_access_rights       = 0x4814,
    Vmcs_guest_cs_access_rights       = 0x4816,
    Vmcs_guest_ss_access_rights       = 0x4818,
    Vmcs_guest_ds_access_rights       = 0x481a,
    Vmcs_guest_fs_access_rights       = 0x481c,
    Vmcs_guest_gs_access_rights       = 0x481e,
    Vmcs_guest_ldtr_access_rights     = 0x4820,
    Vmcs_guest_tr_access_rights       = 0x4822,
    Vmcs_guest_interruptibility_state = 0x4824,
    Vmcs_guest_activity_state         = 0x4826,
    Vmcs_guest_ia32_sysenter_cs       = 0x482a,
    Vmcs_preemption_timer_value       = 0x482e,

    // Must be the last
    Max_32bit_guest
  };

  /**
   * 32-bit host VMCS fields managed by the implementation (not exposed to user
   * space).
   */
  enum Vmcs_32bit_host_fields : Unsigned16
  {
    Vmcs_host_ia32_sysenter_cs = 0x4c00,

    // Must be the last
    Max_32bit_host
  };

  /**
   * Natural-width control VMCS fields supported by the implementation.
   */
  enum Vmcs_nat_ctl_fields : Unsigned16
  {
    Vmcs_cr0_guest_host_mask = 0x6000,
    Vmcs_cr4_guest_host_mask = 0x6002,
    Vmcs_cr0_read_shadow     = 0x6004,
    Vmcs_cr4_read_shadow     = 0x6006,

    // Must be the last
    Max_nat_ctl
  };

  /**
   * Natural-width read-only VMCS fields supported by the implementation.
   */
  enum Vmcs_nat_ro_fields : Unsigned16
  {
    Vmcs_exit_qualification   = 0x6400,
    Vmcs_io_rcx               = 0x6402,
    Vmcs_io_rsi               = 0x6404,
    Vmcs_io_rdi               = 0x6406,
    Vmcs_io_rip               = 0x6408,
    Vmcs_guest_linear_address = 0x640a,

    // Must be the last
    Max_nat_ro
  };

  /**
   * Natural-width guest VMCS fields supported by the implementation.
   */
  enum Vmcs_nat_guest_fields : Unsigned16
  {
    Vmcs_guest_cr0                    = 0x6800,
    Vmcs_guest_cr3                    = 0x6802,
    Vmcs_guest_cr4                    = 0x6804,
    Vmcs_guest_es_base                = 0x6806,
    Vmcs_guest_cs_base                = 0x6808,
    Vmcs_guest_ss_base                = 0x680a,
    Vmcs_guest_ds_base                = 0x680c,
    Vmcs_guest_fs_base                = 0x680e,
    Vmcs_guest_gs_base                = 0x6810,
    Vmcs_guest_ldtr_base              = 0x6812,
    Vmcs_guest_tr_base                = 0x6814,
    Vmcs_guest_gdtr_base              = 0x6816,
    Vmcs_guest_idtr_base              = 0x6818,
    Vmcs_guest_dr7                    = 0x681a,
    Vmcs_guest_rsp                    = 0x681c,
    Vmcs_guest_rip                    = 0x681e,
    Vmcs_guest_rflags                 = 0x6820,
    Vmcs_guest_pending_dbg_exceptions = 0x6822,
    Vmcs_guest_ia32_sysenter_esp      = 0x6824,
    Vmcs_guest_ia32_sysenter_eip      = 0x6826,

    // Must be the last
    Max_nat_guest
  };

  /**
   * Software-defined natural-width guest VMCS fields supported by the
   * implementation.
   *
   * These fields are not present in the hardware VMCS, but represent
   * additional guest state exposed to user space.
   */
  enum Sw_nat_guest_fields : Unsigned16
  {
    Sw_guest_cr2 = 0x6880,
    Sw_nat_arg0  = 0x6882,
    Sw_nat_arg1  = 0x6884,
    Sw_nat_arg2  = 0x6886,
    Sw_nat_arg3  = 0x6888,

    // Must be the last
    Max_nat_sw
  };

  /**
   * Natural-width host VMCS fields managed by the implementation (not exposed
   * to user space).
   */
  enum Vmcs_nat_host_fields : Unsigned16
  {
    Vmcs_host_cr0               = 0x6c00,
    Vmcs_host_cr3               = 0x6c02,
    Vmcs_host_cr4               = 0x6c04,
    Vmcs_host_fs_base           = 0x6c06,
    Vmcs_host_gs_base           = 0x6c08,
    Vmcs_host_tr_base           = 0x6c0a,
    Vmcs_host_gdtr_base         = 0x6c0c,
    Vmcs_host_idtr_base         = 0x6c0e,
    Vmcs_host_ia32_sysenter_esp = 0x6c10,
    Vmcs_host_ia32_sysenter_eip = 0x6c12,
    Vmcs_host_rsp               = 0x6c14,
    Vmcs_host_rip               = 0x6c16,

    // Must be the last
    Max_nat_host
  };
};

INTERFACE [vmx]:

#include <minmax.h>
#include <cxx/dyn_cast>
#include "virt.h"
#include "cpu_lock.h"
#include "warn.h"
#include "cxx/bitfield"
#include "kobject.h"

class Vmx_info;

EXTENSION class Vmx
{
public:
  template<auto field, typename... Fields>
  using if_field_type =
    cxx::enable_if_t<(cxx::is_same_v<decltype(field), Fields> || ...), bool>;

  /*
   * Type-safe VMCS read methods.
   *
   * Note that no methods for reading host-state VMCS fields are implemented.
   * If you plan to introduce such a method, please carefully consider the
   * security implications.
   */

  /**
   * Read a 16-bit guest field from hardware VMCS.
   *
   * \tparam field  Field index.
   *
   * \return Field value.
   */
  template<Vmcs_16bit_guest_fields field>
  static ALWAYS_INLINE Unsigned16 vmcs_read()
  {
    return vmread<Unsigned16>(field);
  }

  /**
   * Read a 64-bit read-only/guest field from hardware VMCS.
   *
   * \tparam field  Field index.
   *
   * \return Field value.
   */
  template<auto field, if_field_type<field, Vmcs_64bit_ro_fields,
                                            Vmcs_64bit_guest_fields> = true>
  static ALWAYS_INLINE Unsigned64 vmcs_read()
  {
    return vmread<Unsigned64>(field);
  }

  /**
   * Read a 32-bit read-only/guest field from hardware VMCS.
   *
   * \tparam field  Field index.
   *
   * \return Field value.
   */
  template<auto field, if_field_type<field, Vmcs_32bit_ro_fields,
                                            Vmcs_32bit_guest_fields> = true>
  static ALWAYS_INLINE Unsigned32 vmcs_read()
  {
    return vmread<Unsigned32>(field);
  }

  /**
   * Read a natural-width read-only/guest field from hardware VMCS.
   *
   * \tparam field  Field index.
   *
   * \return Field value.
   */
  template<auto field, if_field_type<field, Vmcs_nat_ro_fields,
                                            Vmcs_nat_guest_fields> = true>
  static ALWAYS_INLINE Mword vmcs_read()
  {
    return vmread<Mword>(field);
  }

  /*
   * Type-safe VMCS write methods.
   */

  /**
   * Write a 16-bit guest/host field to the hardware VMCS.
   *
   * \tparam field  Field index.
   * \param  value  Value to write to the field.
   */
  template<auto field, if_field_type<field, Vmcs_16bit_guest_fields,
                                            Vmcs_16bit_host_fields> = true>
  static ALWAYS_INLINE void vmcs_write(Unsigned16 value)
  {
    vmwrite<Unsigned16>(field, value);
  }

  /**
   * Write a 64-bit control/guest/host field to the hardware VMCS.
   *
   * \tparam field  Field index.
   * \param  value  Value to write to the field.
   */
  template<auto field, if_field_type<field, Vmcs_64bit_ctl_fields,
                                            Vmcs_64bit_guest_fields,
                                            Vmcs_64bit_host_fields> = true>
  static ALWAYS_INLINE void vmcs_write(Unsigned64 value)
  {
    vmwrite<Unsigned64>(field, value);
  }

  /**
   * Write a 32-bit control/guest/host field to the hardware VMCS.
   *
   * \tparam field  Field index.
   * \param  value  Value to write to the field.
   */
  template<auto field, if_field_type<field, Vmcs_32bit_ctl_fields,
                                            Vmcs_32bit_guest_fields,
                                            Vmcs_32bit_host_fields> = true>
  static ALWAYS_INLINE void vmcs_write(Unsigned32 value)
  {
    vmwrite<Unsigned32>(field, value);
  }

  /**
   * Write a natural-width control/guest/host field to the hardware VMCS.
   *
   * \tparam field  Field index.
   * \param  value  Value to write to the field.
   */
  template<auto field, if_field_type<field, Vmcs_nat_ctl_fields,
                                            Vmcs_nat_guest_fields,
                                            Vmcs_nat_host_fields> = true>
  static ALWAYS_INLINE void vmcs_write(Mword value)
  {
    vmwrite<Mword>(field, value);
  }

  static Per_cpu<Vmx> cpus;
  Vmx_info info;

private:
  /**
   * Physically read a field from the hardware VMCS.
   *
   * This method is for value types smaller or equal to the machine word.
   *
   * Instruction errors are silently ignored.
   *
   * \tparam T      Field value type.
   * \param  field  Field index.
   *
   * \return Field value.
   */
  template<typename T,
           cxx::enable_if_t<sizeof(T) <= sizeof(Mword)>* = nullptr>
  static ALWAYS_INLINE T vmread(Mword field)
  {
    Mword value;

    asm volatile (
      "vmread %[field], %[value]\n\t"
      : [value] "=r" (value)
      : [field] "r" (field)
      : "cc"
    );

    return value;
  }

  /**
   * Physically read a field from the hardware VMCS.
   *
   * This method is for value types larger than the machine word and the read
   * is implemented as two machine word reads.
   *
   * Instruction errors are silently ignored.
   *
   * \tparam T      Field value type.
   * \param  field  Field index.
   *
   * \return Field value.
   */
  template<typename T,
           cxx::enable_if_t<(sizeof(T) == 2 * sizeof(Mword))>* = nullptr>
  static ALWAYS_INLINE T vmread(Mword field)
  {
    Mword value_lo = vmread<Mword>(field);
    Mword value_hi = vmread<Mword>(field + 1);

    return value_lo
      | (static_cast<T>(value_hi) << (sizeof(Mword) * 8));
  }

  /**
   * Physically write a field to the hardware VMCS.
   *
   * This method is for value types smaller or equal to the machine word.
   *
   * If the instruction fails, then \ref vmwrite_failure is set to true.
   *
   * \tparam T      Field value type.
   * \param  field  Field index.
   * \param  value  Field value to write.
   */
  template<typename T,
           cxx::enable_if_t<sizeof(T) <= sizeof(Mword)>* = nullptr>
  static ALWAYS_INLINE void vmwrite(Mword field, T value)
  {
    Mword flags;

    asm volatile (
      "vmwrite %[value], %[field]\n\t"
      "pushf\n\t"
      "pop %[flags]\n\t"
      : [flags] "=r" (flags)
      : [value] "r" (Mword{value}),
        [field] "r" (field)
      : "cc"
    );

    if (EXPECT_FALSE(flags & 0x01))
      {
        WARNX(Info, "VMX: VMfailInvalid vmwrite(0x%04lx, %llx) => %lx\n",
              field, static_cast<Unsigned64>(value), flags);
        vmwrite_failure.current() = true;
      }
    else if (EXPECT_FALSE(flags & 0x40))
      {
        WARNX(Info, "VMX: VMfailValid vmwrite(0x%04lx, %llx) => %lx, "
                    "insn error: 0x%x\n", field, static_cast<Unsigned64>(value),
                    flags, vmread<Unsigned32>(Vmcs_vm_insn_error));
        vmwrite_failure.current() = true;
      }
  }

  /**
   * Physically write a field to the hardware VMCS.
   *
   * This method is for value types larger than the machine word and the write
   * is implemented as two machine word writes.
   *
   * If the instruction fails, then \ref vmwrite_failure is set to true.
   *
   * \tparam T      Field value type.
   * \param  field  Field index.
   * \param  value  Field value to write.
   */
  template<typename T,
           cxx::enable_if_t<(sizeof(T) == 2 * sizeof(Mword))>* = nullptr>
  static ALWAYS_INLINE void vmwrite(Mword field, T value)
  {
    vmwrite<Mword>(field, value);
    vmwrite<Mword>(field + 1, value >> (sizeof(Mword) * 8));
  }

  void *_vmxon;
  Address _vmxon_pa;

  bool _vmx_enabled;
  bool _has_vpid;

  Vmx_vmcs *_current_vmcs;

  /**
   * Flag to indicate whether a VMWRITE instruction has failed.
   *
   * Failing VMWRITE intructions are considered fatal since they might affect
   * the consistency of the host state.
   */
  static Per_cpu<bool> vmwrite_failure;
};

/**
 * Descriptor of offsets/limits for VMCS field groups.
 *
 * Contains the offsets/limits for the control, read-only and guest VMCS fields
 * (for the given field size). The host fields are only stored for kernel
 * purposes (e.g. vCPU states of nested VMs), but they are not exposed to user
 * space.
 */
struct Vmx_field_group_values
{
  Unsigned8 ctl;
  Unsigned8 ro;
  Unsigned8 guest;
  Unsigned8 host;
};

/**
 * Descriptor of index shifts for VMCS field sizes.
 *
 * Contains the index shifts for the 16-bit, 64-bit, 32-bit and natural-width
 * VMCS fields.
 */
struct Vmx_field_size_values
{
  Unsigned8 size_16;
  Unsigned8 size_64;
  Unsigned8 size_32;
  Unsigned8 size_nat;
};

/**
 * Software VMCS field offset table. The memory layout is as follows:
 *
 * 0x00 - 0x03: 4 offsets for 16-bit fields.
 * 0x04 - 0x07: 4 offsets for 64-bit fields.
 * 0x08 - 0x0b: 4 offsets for 32-bit fields.
 * 0x0c - 0x0f: 4 offsets for natural-width fields.
 * 0x10 - 0x13: 4 limits for 16-bit fields.
 * 0x14 - 0x17: 4 limits for 64-bit fields.
 * 0x18 - 0x1b: 4 limits for 32-bit fields.
 * 0x1c - 0x1f: 4 limits for natural-width fields.
 * 0x20 - 0x23: 4 index shifts.
 *        0x24: Offset of the first software VMCS field.
 *        0x25: Size of the software VMCS fields.
 * 0x26 - 0x27: Reserved.
 *
 * The offsets/limits in each size category are in the following order:
 *  - Control fields.
 *  - Read-only fields.
 *  - Guest fields.
 *  - Host fields (never exposed to user space).
 *
 * The index shifts are in the following order:
 *  - 16-bit.
 *  - 64-bit.
 *  - 32-bit.
 *  - Natural-width.
 *
 * All offsets/limits/sizes are represented in a 64-byte granule.
 *
 * The offsets (after being multiplied by 64) are indexes in the `_values`
 * array in \ref Vmx_vm_state_t and bit indexes in the `_dirty_bitmap` array
 * in \ref Vmx_vm_state_t.
 *
 * The limits (after being multiplied by 64) represent the range of the
 * available indexes.
 */
struct Vmx_offset_table
{
  Vmx_field_group_values offsets_16;
  Vmx_field_group_values offsets_64;
  Vmx_field_group_values offsets_32;
  Vmx_field_group_values offsets_nat;

  Vmx_field_group_values limits_16;
  Vmx_field_group_values limits_64;
  Vmx_field_group_values limits_32;
  Vmx_field_group_values limits_nat;

  Vmx_field_size_values index_shifts;

  Unsigned8 base_offset;
  Unsigned8 size;
  Unsigned8 reserved[2];
};

static_assert(sizeof(Vmx_offset_table) == 40,
              "VMX field offset table size is 40 bytes.");

/**
 * Host fields presence discriminator.
 *
 * This enum discriminates whether the VMX extended vCPU state contains host
 * fields or not. Note that the variant of the extended vCPU state that is
 * exposed to user space never exposes host fields.
 *
 * The host fields are only available for kernel purposes (e.g. managing vCPU
 * states of nested VMs).
 */
enum Host_state : bool
{
  Absent = 0,
  Present = 1
};

/**
 * VMX extended vCPU state.
 *
 * This structure is also referred to as "software VMCS" since its purpose is
 * analogous to the hardware VMCS.
 *
 * The memory layout of the VMX extended vCPU state is as follows:
 *
 * 0x000 - 0x007: Reserved.
 * 0x008 - 0x00f: User space data (ignored by the kernel).
 * 0x010 - 0x013: VMCS field index of the software-defined CR2 field in the
 *                software VMCS.
 * 0x014 - 0x017: Reserved.
 * 0x018 - 0x01f: Capability of the hardware VMCS object (with padding).
 * 0x020 - 0x047: Software VMCS field offset table \ref Vmx_offset_table.
 * 0x048 - 0x0bf: Reserved.
 * 0x0c0 - 0xabf: Software VMCS fields (with padding).
 * 0xac0 - 0xbff: Software VMCS fields dirty bitmap (with padding).
 *
 * For completeness, this is the overall memory layout of the vCPU state:
 *
 * 0x000 - 0x1ff: Standard vCPU state \ref Vcpu_state (with padding).
 * 0x200 - 0x3ff: VMX capabilities \ref Vmx_user_info (with padding).
 * 0x400 - 0xfff: VMX extended vCPU state.
 *
 * \tparam HOST_STATE  Host fields presence discriminator.
 */
template<Host_state HOST_STATE = Absent>
class Vmx_vm_state_t
{
private:
  enum
  {
    /**
     * Size of the values ares (in bytes).
     */
    Values_size = 2560,

    /**
     * Size of the dirty bitmap (in bytes).
     */
    Dirty_bitmap_size = Values_size / 8
  };

  static_assert((Values_size % 8) == 0,
                "Values ares properly sized with respect to dirty bitmap.");

  /**
   * Compute VMCS field index base index (within its size and group category).
   *
   * \param field  VMCS field index.
   *
   * \return VMCS field index base index.
   */
  static constexpr unsigned int base_index(Unsigned16 field)
  {
    return field & 0x3feU;
  }

  /**
   * Compute VMCS field index size.
   *
   * \param field  VMCS field index.
   *
   * \retval 0  16-bit field.
   * \retval 1  64-bit field.
   * \retval 2  32-bit field.
   * \retval 3  Natural-width field.
   */
  static constexpr unsigned int size(Unsigned16 field)
  {
    return (field >> 13) & 0x03U;
  }

  /**
   * Compute VMCS field index group.
   *
   * \param field  VMCS field index.
   *
   * \retval 0  Control field.
   * \retval 1  Read-only field.
   * \retval 2  Guest field.
   * \retval 3  Host field.
   */
  static constexpr unsigned int group(Unsigned16 field)
  {
    return (field >> 10) & 0x03U;
  }

  /**
   * Get software VMCS index shift for the given size category.
   *
   * The index shift is used to compute the relative byte offset of the given
   * VMCS field in the software VMCS.
   *
   * \note Canonical VMCS field indexes are always even numbers. Thus the index
   *       shift already accounts for this.
   *
   * \note Natural-width VMCS fields are always stored as 64-bit entries in the
   *       software VMCS.
   *
   * \param size  VMCS field size.
   *
   * \return Index shift.
   */
  static constexpr unsigned int shift(unsigned int size)
  {
    switch (size)
      {
      case 0: /* 16-bits -> shift by 0 */
        return 0;
      case 1: /* 64-bits -> shift by 2 */
        return 2;
      case 2: /* 32-bits -> shift by 1 */
        return 1;
      case 3: /* Natural-width (assuming 64-bits) -> shift by 2 */
        return 2;
      }

    return 0;
  }

  /**
   * Get software VMCS index shift for the given VMCS field index.
   *
   * See \ref shift() for the detailed explanation.
   *
   * \param field  VMCS field index.
   *
   * \return Index shift.
   */
  static constexpr unsigned int field_shift(Unsigned16 field)
  {
    return shift(size(field));
  }

  /**
   * Get the range of the 16-bit VMCS fields supported in the given group.
   *
   * \param group  VMCS field group.
   *
   * \return VMCS fields range for the given group.
   */
  static constexpr unsigned int group_size_16(unsigned int group)
  {
    /*
     * Note: The maximal enum value is the last field plus 1. Therefore we
     * add an extra 1 to it to get the range.
     */
    switch (group)
      {
        case 2:
          return base_index(Vmx::Max_16bit_guest) + 1;
        case 3:
          if (HOST_STATE)
            return base_index(Vmx::Max_16bit_host) + 1;
          else
            return 0;
      }

    return 0;
  }

  /**
   * Get the range of the 64-bit VMCS fields supported in the given group.
   *
   * \param group  VMCS field group.
   *
   * \return VMCS fields range for the given group.
   */
  static constexpr unsigned int group_size_64(unsigned int group)
  {
    /*
     * Note: The maximal enum value is the last field plus 1. Therefore we
     * add an extra 1 to it to get the range.
     */
    switch (group)
      {
        case 0:
          return base_index(Vmx::Max_64bit_ctl) + 1;
        case 1:
          return base_index(Vmx::Max_64bit_ro) + 1;
        case 2:
          return max(base_index(Vmx::Max_64bit_guest),
                     base_index(Vmx::Max_64bit_sw)) + 1;
        case 3:
          if (HOST_STATE)
            return base_index(Vmx::Max_64bit_host) + 1;
          else
            return 0;
      }

    return 0;
  }

  /**
   * Get the range of the 32-bit VMCS fields supported in the given group.
   *
   * \param group  VMCS field group.
   *
   * \return VMCS fields range for the given group.
   */
  static constexpr unsigned int group_size_32(unsigned int group)
  {
    /*
     * Note: The maximal enum value is the last field plus 1. Therefore we
     * add an extra 1 to it to get the range.
     */
    switch (group)
      {
        case 0:
          return base_index(Vmx::Max_32bit_ctl) + 1;
        case 1:
          return base_index(Vmx::Max_32bit_ro) + 1;
        case 2:
          return base_index(Vmx::Max_32bit_guest) + 1;
        case 3:
          if (HOST_STATE)
            return base_index(Vmx::Max_32bit_host) + 1;
          else
            return 0;
      }

    return 0;
  }

  /**
   * Get the range of the natural-width VMCS fields supported in the given
   * group.
   *
   * \param group  VMCS field group.
   *
   * \return VMCS fields range for the given group.
   */
  static constexpr unsigned int group_size_nat(unsigned int group)
  {
    /*
     * Note: The maximal enum value is the last field plus 1. Therefore we
     * add an extra 1 to it to get the range.
     */
    switch (group)
      {
        case 0:
          return base_index(Vmx::Max_nat_ctl) + 1;
        case 1:
          return base_index(Vmx::Max_nat_ro) + 1;
        case 2:
          return max(base_index(Vmx::Max_nat_guest),
                     base_index(Vmx::Max_nat_sw)) + 1;
        case 3:
          if (HOST_STATE)
            return base_index(Vmx::Max_nat_host) + 1;
          else
            return 0;
      }

    return 0;
  }

  /**
   * Compute the size of the software VMCS block for the given size and group
   * category.
   *
   * \note Since the offset table represents offsets/limits/sizes in 64-byte
   *       granules, we align the size up to the nearest 64 bytes.
   *
   * \param size   VMCS field size.
   * \param group  VMCS field group.
   *
   * \return Size of the software VMCS block (in bytes).
   */
  static constexpr unsigned int block_size(unsigned int size,
                                           unsigned int group)
  {
    unsigned int size_size = 0;

    switch (size)
      {
      case 0:
        size_size = group_size_16(group);
        break;
      case 1:
        size_size = group_size_64(group);
        break;
      case 2:
        size_size = group_size_32(group);
        break;
      case 3:
        size_size = group_size_nat(group);
        break;
      }

    return ((size_size << shift(size)) + 63U) & (~63U);
  }

  /**
   * Compute the offset of a VMCS field index in the software VMCS.
   *
   * \param field  VMCS field index.
   *
   * \return Offset in the software VMCS (i.e. byte offset in \ref _values
   *         and bit offset in \ref _dirty_bitmap).
   */
  static constexpr unsigned int offset(Unsigned16 field)
  {
    unsigned int idx = base_index(field);
    unsigned int sz = size(field);
    unsigned int grp = group(field);

    unsigned int offset = 0;

    /*
     * The offset base is calculated by summing up the block sizes of the
     * categories that precede the category of the VMCS field index at hand.
     */

    for (unsigned int g = 0; g <= 3; ++g)
      {
        for (unsigned int s = 0; s <= 3; ++s)
          {
            if (g == grp && s == sz)
              return offset + (idx << shift(sz));

            offset += block_size(s, g);
          }
      }

    return offset;
  }

  /**
   * Compute the range the category of the VMCS field index.
   *
   * \param field  VMCS field index.
   *
   * \return Range of category of the VMCS field index.
   */
  static constexpr unsigned int limit(Unsigned16 field)
  {
    unsigned int sz = size(field);
    unsigned int grp = group(field);

    return block_size(sz, grp);
  }

  Unsigned64 _reserved0;
  Unsigned64 _user_data;
  Unsigned32 _cr2_field;
  Unsigned8 _reserved1[4];
  L4_obj_ref _vmcs;

  /*
   * Since the capability type size depends on the platform, align the offset
   * table on the nearest 64-bit boundary to add proper padding.
   */

  alignas(8) Vmx_offset_table _offset_table;

  Unsigned8 _reserved2[120];

  Unsigned8 _values[Values_size];
  Unsigned8 _dirty_bitmap[Dirty_bitmap_size];
};

using Vmx_vm_state = Vmx_vm_state_t<Absent>;

static_assert(sizeof(Vmx_vm_state) + Config::Ext_vcpu_state_offset == 4096,
              "VMX extended VM state fits exactly into 4096 bytes.");

/**
 * Representation of the bitfields of the VM-entry interruption information
 * VMCS field.
 */
struct Vmx_vm_entry_interrupt_info
{
  Unsigned32 value;

  CXX_BITFIELD_MEMBER(0, 7, vector, value);
  CXX_BITFIELD_MEMBER(8, 10, type, value);

  CXX_BITFIELD_MEMBER(10, 10, deliver_insn_length, value);
  CXX_BITFIELD_MEMBER(11, 11, deliver_error_code, value);

  /**
   * This is a Fiasco-specific flag within the reserved bitfield
   * that must be never written to the hardware VMCS field.
   */
  CXX_BITFIELD_MEMBER(30, 30, immediate_exit, value);

  CXX_BITFIELD_MEMBER(31, 31, valid, value);
};

/**
 * Kernel object for the VMX interface which represents the hardware Virtual
 * Machine Control Structure (VMCS). This kernel object is exposed to user
 * space via the vCPU context label.
 *
 * The VMX extended vCPU needs to be assigned a vCPU context, i.e. an instance
 * of this VMCS object (via the _vmcs member in \ref Vmx_vm_state) before it
 * can be resumed. The relationship policy between vCPUs and vCPU contexts is
 * managed by user space.
 */
class Vmx_vmcs :
  public cxx::Dyn_castable<Vmx_vmcs, Kobject>,
  public Ref_cnt_obj
{
public:
  enum
  {
    /**
     * Nominal hardware VMCS size (actual size may be smaller).
     */
    Vmcs_size = 0x1000,

    /**
     * Hardware VMCS order.
     */
    Vmcs_order = 12
  };

private:
  /**
   * VMCS region.
   */
  void *_ptr;

  /**
   * Allocation quota.
   */
  Ram_quota *_quota;

  /**
   * Extended vCPU state to which the VMCS object is bound to.
   */
  Vmx_vm_state *_owner;

  /**
   * CPU where the VMCS region is active.
   */
  Cpu_number _active;
};

// -----------------------------------------------------------------------
IMPLEMENTATION[vmx]:

#include <cstring>

#include "atomic.h"
#include "cpu.h"
#include "cpu_call.h"
#include "cpu_mask.h"
#include "entry-ia32.h"
#include "idt.h"
#include "kmem.h"
#include "kmem_alloc.h"
#include "kmem_slab.h"
#include "kobject_dbg.h"
#include "l4_types.h"
#include "msrdefs.h"
#include "vm_vmx_asm.h"

JDB_DEFINE_TYPENAME(Vmx_vmcs, "VMX VMCS");

/**
 * Initialize the current hardware VMCS.
 *
 * This initializes the host and immutable guest fields of the current hardware
 * VMCS to well-defined values.
 */
PRIVATE inline NEEDS["idt.h", "entry-ia32.h"]
void
Vmx::initialize_current_vmcs()
{
  vmcs_write<Vmcs_host_es_selector>(GDT_DATA_KERNEL);
  vmcs_write<Vmcs_host_cs_selector>(GDT_CODE_KERNEL);
  vmcs_write<Vmcs_host_ss_selector>(GDT_DATA_KERNEL);
  vmcs_write<Vmcs_host_ds_selector>(GDT_DATA_KERNEL);

  /* set FS and GS to unusable in the host state */
  vmcs_write<Vmcs_host_fs_selector>(0);
  vmcs_write<Vmcs_host_gs_selector>(0);
  vmcs_write<Vmcs_host_tr_selector>(Cpu::get_tr());

  vmcs_write<Vmcs_host_rip>(reinterpret_cast<Mword>(vm_vmx_exit_vec));
  vmcs_write<Vmcs_host_ia32_sysenter_cs>(Gdt::gdt_code_kernel);
  vmcs_write<Vmcs_host_ia32_sysenter_eip>
            (reinterpret_cast<Mword>(entry_sys_fast_ipc_c));

  Pseudo_descriptor pseudo;
  Idt::get(&pseudo);
  vmcs_write<Vmcs_host_idtr_base>(pseudo.base());

  // init static guest area stuff
  vmcs_write<Vmcs_link_pointer>(~0ULL);
  vmcs_write<Vmcs_cr3_target_cnt>(0);

  // MSR load / store disabled
  vmcs_write<Vmcs_exit_msr_load_cnt>(0);
  vmcs_write<Vmcs_exit_msr_store_cnt>(0);
  vmcs_write<Vmcs_entry_msr_load_cnt>(0);
}

DEFINE_PER_CPU Per_cpu<Vmx> Vmx::cpus(Per_cpu_data::Cpu_num);
DEFINE_PER_CPU Per_cpu<bool> Vmx::vmwrite_failure;

PUBLIC
bool
Vmx_info::init(Cpu_number cpu)
{
  enum { ShowDebugDumps = 0 };

  basic = Cpu::rdmsr(Msr::Ia32_vmx_basic);
  pinbased_ctls = Cpu::rdmsr(Msr::Ia32_vmx_pinbased_ctls);
  pinbased_ctls_default1 = pinbased_ctls.must_be_one();
  procbased_ctls = Cpu::rdmsr(Msr::Ia32_vmx_procbased_ctls);
  procbased_ctls_default1 = procbased_ctls.must_be_one();
  exit_ctls = Cpu::rdmsr(Msr::Ia32_vmx_exit_ctls);
  exit_ctls_default1 = exit_ctls.must_be_one();
  entry_ctls = Cpu::rdmsr(Msr::Ia32_vmx_entry_ctls);
  entry_ctls_default1 = entry_ctls.must_be_one();
  misc = Cpu::rdmsr(Msr::Ia32_vmx_misc);

  cr0_defs = Bit_defs<Mword>(Cpu::rdmsr(Msr::Ia32_vmx_cr0_fixed0),
                             Cpu::rdmsr(Msr::Ia32_vmx_cr0_fixed1));
  cr4_defs = Bit_defs<Mword>(Cpu::rdmsr(Msr::Ia32_vmx_cr4_fixed0),
                             Cpu::rdmsr(Msr::Ia32_vmx_cr4_fixed1));
  exception_bitmap = Bit_defs_32<Vmx_info::Exceptions>(0xffffffff00000000ULL);

  max_index = Cpu::rdmsr(Msr::Ia32_vmx_vmcs_enum);

  assert((Vmx::Sw_guest_xcr0 & 0x3ff) > max_index);
  assert((Vmx::Sw_guest_cr2 & 0x3ff) > max_index);

  if (basic & (1ULL << 55))
    {
      // do not use the true pin-based ctls because user-level then needs to
      // be aware of the fact that it has to set bits 1, 2, and 4 to default 1
      if constexpr (0) pinbased_ctls = Cpu::rdmsr(Msr::Ia32_vmx_true_pinbased_ctls);

      procbased_ctls = Cpu::rdmsr(Msr::Ia32_vmx_true_procbased_ctls);
      exit_ctls = Cpu::rdmsr(Msr::Ia32_vmx_true_exit_ctls);
      entry_ctls = Cpu::rdmsr(Msr::Ia32_vmx_true_entry_ctls);
    }

  if constexpr (ShowDebugDumps)
    dump("as read from hardware");

  // Since we require EPT support, we also require the secondary
  // processor-based VM-execution controls.
  if (!procbased_ctls.allowed(Vmx_info::PRB1_enable_proc_based_ctls_2))
    {
      if (cpu == Cpu_number::boot_cpu())
        printf("VMX: Secondary processor-based VM-execution controls not available\n");

      return false;
    }

  procbased_ctls.enforce(Vmx_info::PRB1_enable_proc_based_ctls_2, true);
  procbased_ctls2 = Cpu::rdmsr(Msr::Ia32_vmx_procbased_ctls2);

  if (!procbased_ctls2.allowed(Vmx_info::PRB2_enable_ept))
    {
      if (cpu == Cpu_number::boot_cpu())
        printf("VMX: EPT not available\n");

      return false;
    }

  procbased_ctls2.enforce(Vmx_info::PRB2_enable_ept, true);
  ept_vpid_cap = Cpu::rdmsr(Msr::Ia32_vmx_ept_vpid_cap);

  if (has_invept() && !has_invept_global())
    {
      // Having the INVEPT instruction indicates that some kind of EPT TLB
      // flushing is required. If the global INVEPT type is not supported, we
      // cannot currently ensure proper EPT TLB flushing. This is presumably
      // an extremely rare scenario.
      if (cpu == Cpu_number::boot_cpu())
        printf("VMX: Cannot ensure EPT TLB flushing\n");

      return false;
    }

  pinbased_ctls.enforce(Vmx_info::PIB_ext_int_exit);
  pinbased_ctls.enforce(Vmx_info::PIB_nmi_exit);

  // currently we IO-passthrough is missing, disable I/O bitmaps and enforce
  // unconditional io exiting
  procbased_ctls.enforce(Vmx_info::PRB1_use_io_bitmaps, false);
  procbased_ctls.enforce(Vmx_info::PRB1_unconditional_io_exit);

  // Always exit if the guest accesses a debug register.
  procbased_ctls.enforce(Vmx_info::PRB1_mov_dr_exit, true);

  procbased_ctls.enforce(Vmx_info::PRB1_use_msr_bitmaps, false);

  // exit on performance counter use
  procbased_ctls.enforce(Vmx_info::PRB1_rdpmc_exiting, true);

  // virtual APIC not yet supported
  procbased_ctls.enforce(Vmx_info::PRB1_tpr_shadow, false);

  // we disable VPID so far, need to handle virtualize it in Fiasco,
  // as done for AMDs ASIDs
  procbased_ctls2.enforce(Vmx_info::PRB2_enable_vpid, false);

  // we do not (yet) support Page Modification Logging (PML)
  procbased_ctls2.enforce(Vmx_info::PRB2_enable_pml, false);

  if (procbased_ctls2.allowed(Vmx_info::PRB2_unrestricted))
    {
      // unrestricted guest allows PE and PG to be 0
      cr0_defs.relax(0);  // PE
      cr0_defs.relax(31); // PG
      procbased_ctls2.enforce(Vmx_info::PRB2_unrestricted);
    }
  else
    {
      assert(not cr0_defs.allowed(0, false));
      assert(not cr0_defs.allowed(31, false));
    }

  // We currently do not implement the xss bitmap, and do not support
  // the Msr::Ia32_xss which is shared between guest and host. Therefore
  // we disable xsaves/xrstores for the guest.
  procbased_ctls2.enforce(Vmx_info::PRB2_enable_xsaves, false);

  // never automatically ack interrupts on exit
  exit_ctls.enforce(Vmx_info::Ex_ack_irq_on_exit, false);

  // host-state is 64bit or not
  exit_ctls.enforce(Vmx_info::Ex_host_addr_size, sizeof(long) > sizeof(int));

  // allow cr4.vmxe
  cr4_defs.relax(13);

  exception_bitmap.enforce(Vmx_info::Exception_db, true);
  exception_bitmap.enforce(Vmx_info::Exception_ac, true);

  if constexpr (ShowDebugDumps)
    dump("as modified");

  return true;
}

PUBLIC
void
Vmx_info::dump(const char *tag) const
{
  printf("VMX MSRs %s:\n", tag);
  printf("basic                = %16llx\n", basic);
  pinbased_ctls.print("pinbased_ctls");
  procbased_ctls.print("procbased_ctls");
  exit_ctls.print("exit_ctls");
  entry_ctls.print("entry_ctls");
  printf("misc                 = %16llx\n", misc);
  cr0_defs.print("cr0_fixed");
  cr4_defs.print("cr4_fixed");
  procbased_ctls2.print("procbased_ctls2");
  printf("ept_vpid_cap         = %16llx\n", ept_vpid_cap);
  exception_bitmap.print("exception_bitmap");
}

/**
 * Initialize the VMX extended vCPU state.
 */
PUBLIC
template<Host_state HOST_STATE>
void
Vmx_vm_state_t<HOST_STATE>::init()
{
  static_assert(offset(0x6c00) + limit(0x6c00) < Values_size,
                "Field offsets fit within the software VMCS.");

  _cr2_field = Vmx::Sw_guest_cr2;
  _offset_table =
    {
      {
        /* 16-bit offsets */
        offset(0x0000) / 64,
        offset(0x0400) / 64,
        offset(0x0800) / 64,
        offset(0x0c00) / 64,
      },
      {
        /* 64-bit offsets */
        offset(0x2000) / 64,
        offset(0x2400) / 64,
        offset(0x2800) / 64,
        offset(0x2c00) / 64,
      },
      {
        /* 32-bit offsets */
        offset(0x4000) / 64,
        offset(0x4400) / 64,
        offset(0x4800) / 64,
        offset(0x4c00) / 64,
      },
      {
        /* Natural-width offsets */
        offset(0x6000) / 64,
        offset(0x6400) / 64,
        offset(0x6800) / 64,
        offset(0x6c00) / 64,
      },
      {
        /* 16-bit limits */
        limit(0x0000) / 64,
        limit(0x0400) / 64,
        limit(0x0800) / 64,
        limit(0x0c00) / 64,
      },
      {
        /* 64-bit limits */
        limit(0x2000) / 64,
        limit(0x2400) / 64,
        limit(0x2800) / 64,
        limit(0x2c00) / 64,
      },
      {
        /* 32-bit limits */
        limit(0x4000) / 64,
        limit(0x4400) / 64,
        limit(0x4800) / 64,
        limit(0x4c00) / 64,
      },
      {
        /* Natural-width limits */
        limit(0x6000) / 64,
        limit(0x6400) / 64,
        limit(0x6800) / 64,
        limit(0x6c00) / 64,
      },
      {
        /* Index shifts */
        shift(0),
        shift(1),
        shift(2),
        shift(3)
      },

      /* Offset of the first field */
      offset(0x0000) / 64,

      /* Size of the values area */
      (offset(0x6c00) + limit(0x6c00) - offset(0x0000)) / 64,

      {0, 0}
    };

  _vmcs = L4_obj_ref::Invalid;
  memset(_dirty_bitmap, 0, sizeof(_dirty_bitmap));
  memset(_values, 0, sizeof(_values));
}

/*
 * Type-safe methods for reading the software VMCS.
 *
 * The goal is to make the methods inlineable, with the offset calculation done
 * at compile time. There are additional paranoid compile-time assertions that
 * make sure the constants are defined correctly.
 */

/**
 * Read a 16-bit guest field from the software VMCS.
 *
 * \tparam field  VMCS field index.
 *
 * \return Field value.
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_16bit_guest_fields field>
Unsigned16
Vmx_vm_state_t<HOST_STATE>::read() const
{
  static_assert(size(field) == 0);
  static_assert(group(field) == 2);

  constexpr unsigned int off = offset(field);
  Unsigned16 *ptr = offset_cast<Unsigned16 *>(_values, off);
  return *ptr;
}

/**
 * Read a 64-bit control field from the software VMCS.
 *
 * \tparam field  VMCS field index.
 *
 * \return Field value.
 */
PUBLIC inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_64bit_ctl_fields field>
Unsigned64
Vmx_vm_state_t<HOST_STATE>::read() const
{
  static_assert(size(field) == 1);
  static_assert(group(field) == 0);

  constexpr unsigned int off = offset(field);
  Unsigned64 *ptr = offset_cast<Unsigned64 *>(_values, off);
  return *ptr;
}

/**
 * Read a 64-bit guest (including software-defined) field from the software
 * VMCS.
 *
 * \tparam field  VMCS field index.
 *
 * \return Field value.
 */
PUBLIC inline
template<Host_state HOST_STATE>
template<auto field, Vmx::if_field_type<field, Vmx::Vmcs_64bit_guest_fields,
                                               Vmx::Sw_64bit_guest_fields> = true>
Unsigned64
Vmx_vm_state_t<HOST_STATE>::read() const
{
  static_assert(size(field) == 1);
  static_assert(group(field) == 2);

  constexpr unsigned int off = offset(field);
  Unsigned64 *ptr = offset_cast<Unsigned64 *>(_values, off);
  return *ptr;
}

/**
 * Read a 32-bit control field from the software VMCS.
 *
 * \tparam field  VMCS field index.
 *
 * \return Field value.
 */
PUBLIC inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_32bit_ctl_fields field>
Unsigned32
Vmx_vm_state_t<HOST_STATE>::read() const
{
  static_assert(size(field) == 2);
  static_assert(group(field) == 0);

  constexpr unsigned int off = offset(field);
  Unsigned32 *ptr = offset_cast<Unsigned32 *>(_values, off);
  return *ptr;
}

/**
 * Read a 32-bit read-only field from the software VMCS.
 *
 * \tparam field  VMCS field index.
 *
 * \return Field value.
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_32bit_ro_fields field>
Unsigned32
Vmx_vm_state_t<HOST_STATE>::read() const
{
  static_assert(size(field) == 2);
  static_assert(group(field) == 1);

  constexpr unsigned int off = offset(field);
  Unsigned32 *ptr = offset_cast<Unsigned32 *>(_values, off);
  return *ptr;
}

/**
 * Read a 32-bit guest field from the software VMCS.
 *
 * \tparam field  VMCS field index.
 *
 * \return Field value.
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_32bit_guest_fields field>
Unsigned32
Vmx_vm_state_t<HOST_STATE>::read() const
{
  static_assert(size(field) == 2);
  static_assert(group(field) == 2);

  constexpr unsigned int off = offset(field);
  Unsigned32 *ptr = offset_cast<Unsigned32 *>(_values, off);
  return *ptr;
}

/**
 * Read a natural-width control field from the software VMCS.
 *
 * \tparam field  VMCS field index.
 *
 * \return Field value.
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_nat_ctl_fields field>
Mword
Vmx_vm_state_t<HOST_STATE>::read() const
{
  static_assert(size(field) == 3);
  static_assert(group(field) == 0);

  constexpr unsigned int off = offset(field);
  Mword *ptr = offset_cast<Mword *>(_values, off);
  return *ptr;
}

/**
 * Read a natural-width guest (including software-defined) field from the
 * software VMCS.
 *
 * \tparam field  VMCS field index.
 *
 * \return Field value.
 */
PUBLIC inline
template<Host_state HOST_STATE>
template<auto field, Vmx::if_field_type<field, Vmx::Vmcs_nat_guest_fields,
                                               Vmx::Sw_nat_guest_fields> = true>
Mword
Vmx_vm_state_t<HOST_STATE>::read() const
{
  static_assert(size(field) == 3);
  static_assert(group(field) == 2);

  constexpr unsigned int off = offset(field);
  Mword *ptr = offset_cast<Mword *>(_values, off);
  return *ptr;
}

/*
 * Type-safe methods for writing the software VMCS.
 *
 * The goal is to make the methods inlineable, with the offset calculation done
 * at compile time. There are additional paranoid compile-time assertions that
 * make sure the constants are defined correctly.
 */

/**
 * Write a 16-bit guest field to the software VMCS.
 *
 * \tparam field  VMCS field index.
 * \param  value  Field value to write.
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_16bit_guest_fields field>
void
Vmx_vm_state_t<HOST_STATE>::write(Unsigned16 value)
{
  static_assert(size(field) == 0);
  static_assert(group(field) == 2);

  constexpr unsigned int off = offset(field);
  Unsigned16 *ptr = offset_cast<Unsigned16 *>(_values, off);
  *ptr = value;
}

/**
 * Write a 64-bit read-only field to the software VMCS.
 *
 * \tparam field  VMCS field index.
 * \param  value  Field value to write.
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_64bit_ro_fields field>
void
Vmx_vm_state_t<HOST_STATE>::write(Unsigned64 value)
{
  static_assert(size(field) == 1);
  static_assert(group(field) == 1);

  constexpr unsigned int off = offset(field);
  Unsigned64 *ptr = offset_cast<Unsigned64 *>(_values, off);
  *ptr = value;
}

/**
 * Write a 64-bit guest (including software-defined) field to the software
 * VMCS.
 *
 * \tparam field  VMCS field index.
 * \param  value  Field value to write.
 */
PUBLIC inline
template<Host_state HOST_STATE>
template<auto field, Vmx::if_field_type<field, Vmx::Vmcs_64bit_guest_fields,
                                               Vmx::Sw_64bit_guest_fields> = true>
void
Vmx_vm_state_t<HOST_STATE>::write(Unsigned64 value)
{
  static_assert(size(field) == 1);
  static_assert(group(field) == 2);

  constexpr unsigned int off = offset(field);
  Unsigned64 *ptr = offset_cast<Unsigned64 *>(_values, off);
  *ptr = value;
}

/**
 * Write a 32-bit control field to the software VMCS.
 *
 * \tparam field  VMCS field index.
 * \param  value  Field value to write.
 */
PUBLIC inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_32bit_ctl_fields field>
void
Vmx_vm_state_t<HOST_STATE>::write(Unsigned32 value)
{
  static_assert(size(field) == 2);
  static_assert(group(field) == 0);

  constexpr unsigned int off = offset(field);
  Unsigned32 *ptr = offset_cast<Unsigned32 *>(_values, off);
  *ptr = value;
}

/**
 * Write a 32-bit read-only field to the software VMCS.
 *
 * \tparam field  VMCS field index.
 * \param  value  Field value to write.
 */
PUBLIC inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_32bit_ro_fields field>
void
Vmx_vm_state_t<HOST_STATE>::write(Unsigned32 value)
{
  static_assert(size(field) == 2);
  static_assert(group(field) == 1);

  constexpr unsigned int off = offset(field);
  Unsigned32 *ptr = offset_cast<Unsigned32 *>(_values, off);
  *ptr = value;
}

/**
 * Write a 32-bit guest field to the software VMCS.
 *
 * \tparam field  VMCS field index.
 * \param  value  Field value to write.
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_32bit_guest_fields field>
void
Vmx_vm_state_t<HOST_STATE>::write(Unsigned32 value)
{
  static_assert(size(field) == 2);
  static_assert(group(field) == 2);

  constexpr unsigned int off = offset(field);
  Unsigned32 *ptr = offset_cast<Unsigned32 *>(_values, off);
  *ptr = value;
}

/**
 * Write a natural-width read-only field to the software VMCS.
 *
 * \tparam field  VMCS field index.
 * \param  value  Field value to write.
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_nat_ro_fields field>
void
Vmx_vm_state_t<HOST_STATE>::write(Mword value)
{
  static_assert(size(field) == 3);
  static_assert(group(field) == 1);

  constexpr unsigned int off = offset(field);
  Mword *ptr = offset_cast<Mword *>(_values, off);
  *ptr = value;
}

/**
 * Write a natural-width guest (including software-defined) field to the
 * software VMCS.
 *
 * \tparam field  VMCS field index.
 * \param  value  Field value to write.
 */
PUBLIC inline
template<Host_state HOST_STATE>
template<auto field, Vmx::if_field_type<field, Vmx::Vmcs_nat_guest_fields,
                                               Vmx::Sw_nat_guest_fields> = true>
void
Vmx_vm_state_t<HOST_STATE>::write(Mword value)
{
  static_assert(size(field) == 3);
  static_assert(group(field) == 2);

  constexpr unsigned int off = offset(field);
  Mword *ptr = offset_cast<Mword *>(_values, off);
  *ptr = value;
}

/*
 * Type-safe methods for examining the dirty bitmap.
 */

/**
 * Get and clear the dirty bit of a field.
 *
 * \tparam field  VMCS field index to examine.
 *
 * \return Value of the dirty bit before clearing.
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<auto field, Vmx::if_field_type<field, Vmx::Vmcs_16bit_guest_fields,
                                               Vmx::Vmcs_64bit_ctl_fields,
                                               Vmx::Vmcs_64bit_guest_fields,
                                               Vmx::Vmcs_32bit_ctl_fields,
                                               Vmx::Vmcs_32bit_guest_fields,
                                               Vmx::Vmcs_nat_ctl_fields,
                                               Vmx::Vmcs_nat_guest_fields> = true>
bool
Vmx_vm_state_t<HOST_STATE>::clear()
{
  constexpr unsigned int off = offset(field);
  unsigned int index = off / 8;
  Unsigned8 mask = 1U << (off % 8);

  bool dirty = _dirty_bitmap[index] & mask;
  if (dirty)
    _dirty_bitmap[index] &= ~mask;

  return dirty;
}

/*
 * Type-safe methods for copying fields from software VMCS to hardware VMCS.
 */

/**
 * Copy a field from software VMCS to hardware VMCS.
 *
 * If the field is not dirty, then it is not copied.
 *
 * The following field types are supported: 16-bit guest, 64-bit control/guest,
 * 32-bit control/guest, natural-width control/guest.
 *
 * \tparam field  Field to copy.
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<auto field, Vmx::if_field_type<field, Vmx::Vmcs_16bit_guest_fields,
                                               Vmx::Vmcs_64bit_ctl_fields,
                                               Vmx::Vmcs_64bit_guest_fields,
                                               Vmx::Vmcs_32bit_ctl_fields,
                                               Vmx::Vmcs_32bit_guest_fields,
                                               Vmx::Vmcs_nat_ctl_fields,
                                               Vmx::Vmcs_nat_guest_fields> = true>
void
Vmx_vm_state_t<HOST_STATE>::to_vmcs()
{
  if (!clear<field>())
    return;

  Vmx::vmcs_write<field>(read<field>());
}

/**
 * Copy a 32-bit control field from software VMCS to hardware VMCS with
 * masking.
 *
 * Due to the masking, the value is copied regardless of the dirty status.
 *
 * \tparam field  Field to copy.
 * \param  mask   Mask to be applied on the value before writing to the
 *                hardware VMCS.
 *
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_32bit_ctl_fields field>
Vmx_info::Flags<Unsigned32>
Vmx_vm_state_t<HOST_STATE>::to_vmcs(Vmx_info::Bit_defs<Unsigned32> const &mask)
{
  clear<field>();
  Unsigned32 res = mask.apply(read<field>());
  Vmx::vmcs_write<field>(res);
  return Vmx_info::Flags<Unsigned32>(res);
}

/**
 * Copy a natural-width guest field from software VMCS to hardware VMCS with
 * masking.
 *
 * Due to the masking, the value is copied regardless of the dirty status.
 *
 * \tparam field  Field to copy.
 * \param  mask   Mask to be applied on the value before writing to the
 *                hardware VMCS.
 *
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<Vmx::Vmcs_nat_guest_fields field>
Vmx_info::Flags<Mword>
Vmx_vm_state_t<HOST_STATE>::to_vmcs(Vmx_info::Bit_defs<Mword> const &mask)
{
  clear<field>();
  Mword res = mask.apply(read<field>());
  Vmx::vmcs_write<field>(res);
  return Vmx_info::Flags<Mword>(res);
}

/*
 * Type-safe methods for copying fields from hardware VMCS to software VMCS.
 */

/**
 * Copy a field from hardware VMCS to software VMCS.
 *
 * The following field types are supported: 16-bit guest,
 * 32-bit read-only/guest, 64-bit read-only/guest,
 * natural-width read-only/guest.
 *
 * \tparam field  Field to copy.
 */
PRIVATE inline
template<Host_state HOST_STATE>
template<auto field, Vmx::if_field_type<field, Vmx::Vmcs_16bit_guest_fields,
                                               Vmx::Vmcs_32bit_ro_fields,
                                               Vmx::Vmcs_32bit_guest_fields,
                                               Vmx::Vmcs_64bit_ro_fields,
                                               Vmx::Vmcs_64bit_guest_fields,
                                               Vmx::Vmcs_nat_ro_fields,
                                               Vmx::Vmcs_nat_guest_fields> = true>
void
Vmx_vm_state_t<HOST_STATE>::from_vmcs()
{
  write<field>(Vmx::vmcs_read<field>());
}

/**
 * Move the guest state from software VMCS to hardware VMCS.
 */
PUBLIC inline
template<Host_state HOST_STATE>
void
Vmx_vm_state_t<HOST_STATE>::load_guest_state()
{
  Cpu_number const cpu = current_cpu();
  Vmx &vmx = Vmx::cpus.cpu(cpu);

  // read VM-entry controls, apply filter and keep for later
  Vmx_info::Flags<Unsigned32> entry_ctls
    = to_vmcs<Vmx::Vmcs_vm_entry_ctls>(vmx.info.entry_ctls);

  Vmx_info::Flags<Unsigned32> pinbased_ctls
    = to_vmcs<Vmx::Vmcs_pin_based_vm_exec_ctls>(vmx.info.pinbased_ctls);

  to_vmcs<Vmx::Vmcs_pri_proc_based_vm_exec_ctls>(vmx.info.procbased_ctls);

  Vmx_info::Flags<Unsigned32> procbased_ctls_2
    = to_vmcs<Vmx::Vmcs_sec_proc_based_vm_exec_ctls>(vmx.info.procbased_ctls2);

  to_vmcs<Vmx::Vmcs_vm_exit_ctls>(vmx.info.exit_ctls);

  // write 16-bit fields
  to_vmcs<Vmx::Vmcs_guest_es_selector>();
  to_vmcs<Vmx::Vmcs_guest_cs_selector>();
  to_vmcs<Vmx::Vmcs_guest_ss_selector>();
  to_vmcs<Vmx::Vmcs_guest_ds_selector>();
  to_vmcs<Vmx::Vmcs_guest_fs_selector>();
  to_vmcs<Vmx::Vmcs_guest_gs_selector>();
  to_vmcs<Vmx::Vmcs_guest_ldtr_selector>();
  to_vmcs<Vmx::Vmcs_guest_tr_selector>();
  to_vmcs<Vmx::Vmcs_guest_interrupt_status>();

  // write 64-bit fields
  to_vmcs<Vmx::Vmcs_guest_ia32_debugctl>();

  // check if the following bits are allowed to be set in entry_ctls
  if (entry_ctls.test(Vmx_info::En_load_perf_global_ctl))
    to_vmcs<Vmx::Vmcs_guest_ia32_perf_global_ctrl>();

  if (entry_ctls.test(Vmx_info::En_load_ia32_pat))
    to_vmcs<Vmx::Vmcs_guest_ia32_pat>();

  if (entry_ctls.test(Vmx_info::En_load_ia32_efer))
    to_vmcs<Vmx::Vmcs_guest_ia32_efer>();

  // write 32-bit fields
  to_vmcs<Vmx::Vmcs_guest_es_limit>();
  to_vmcs<Vmx::Vmcs_guest_cs_limit>();
  to_vmcs<Vmx::Vmcs_guest_ss_limit>();
  to_vmcs<Vmx::Vmcs_guest_ds_limit>();
  to_vmcs<Vmx::Vmcs_guest_fs_limit>();
  to_vmcs<Vmx::Vmcs_guest_gs_limit>();
  to_vmcs<Vmx::Vmcs_guest_ldtr_limit>();
  to_vmcs<Vmx::Vmcs_guest_tr_limit>();
  to_vmcs<Vmx::Vmcs_guest_gdtr_limit>();
  to_vmcs<Vmx::Vmcs_guest_idtr_limit>();
  to_vmcs<Vmx::Vmcs_guest_es_access_rights>();
  to_vmcs<Vmx::Vmcs_guest_cs_access_rights>();
  to_vmcs<Vmx::Vmcs_guest_ss_access_rights>();
  to_vmcs<Vmx::Vmcs_guest_ds_access_rights>();
  to_vmcs<Vmx::Vmcs_guest_fs_access_rights>();
  to_vmcs<Vmx::Vmcs_guest_gs_access_rights>();
  to_vmcs<Vmx::Vmcs_guest_ldtr_access_rights>();
  to_vmcs<Vmx::Vmcs_guest_tr_access_rights>();
  to_vmcs<Vmx::Vmcs_guest_interruptibility_state>();
  to_vmcs<Vmx::Vmcs_guest_activity_state>();
  to_vmcs<Vmx::Vmcs_guest_ia32_sysenter_cs>();

  if (pinbased_ctls.test(Vmx_info::PIB_preemption_timer))
    to_vmcs<Vmx::Vmcs_preemption_timer_value>();

  // write natural-width fields
  to_vmcs<Vmx::Vmcs_guest_cr0>(vmx.info.cr0_defs);
  to_vmcs<Vmx::Vmcs_guest_cr4>(vmx.info.cr4_defs);
  to_vmcs<Vmx::Vmcs_guest_es_base>();
  to_vmcs<Vmx::Vmcs_guest_cs_base>();
  to_vmcs<Vmx::Vmcs_guest_ss_base>();
  to_vmcs<Vmx::Vmcs_guest_ds_base>();
  to_vmcs<Vmx::Vmcs_guest_fs_base>();
  to_vmcs<Vmx::Vmcs_guest_gs_base>();
  to_vmcs<Vmx::Vmcs_guest_ldtr_base>();
  to_vmcs<Vmx::Vmcs_guest_tr_base>();
  to_vmcs<Vmx::Vmcs_guest_gdtr_base>();
  to_vmcs<Vmx::Vmcs_guest_idtr_base>();
  to_vmcs<Vmx::Vmcs_guest_dr7>();
  to_vmcs<Vmx::Vmcs_guest_rsp>();
  to_vmcs<Vmx::Vmcs_guest_rip>();
  to_vmcs<Vmx::Vmcs_guest_rflags>();
  to_vmcs<Vmx::Vmcs_guest_pending_dbg_exceptions>();
  to_vmcs<Vmx::Vmcs_guest_ia32_sysenter_esp>();
  to_vmcs<Vmx::Vmcs_guest_ia32_sysenter_eip>();

  // TODO: The following features are ignored as they need to be handled in
  // a virtualized fashion by Fiasco:
  //
  //   * VPIDs
  //   * I/O bitmaps
  //   * MSR bitmaps
  //   * SMM virtualization
  //   * Virtual APIC
  //   * PAE handling without EPT

  to_vmcs<Vmx::Vmcs_tsc_offset>();

  if (procbased_ctls_2.test(Vmx_info::PRB2_tsc_scaling))
    to_vmcs<Vmx::Vmcs_tsc_multiplier>();

  if (procbased_ctls_2.test(Vmx_info::PRB2_virtualize_apic))
    to_vmcs<Vmx::Vmcs_apic_access_address>();

  // exception bit map and pf error-code stuff
  to_vmcs<Vmx::Vmcs_exception_bitmap>(vmx.info.exception_bitmap);
  to_vmcs<Vmx::Vmcs_page_fault_error_mask>();
  to_vmcs<Vmx::Vmcs_page_fault_error_match>();

  // VM entry control stuff
  Vmx_vm_entry_interrupt_info int_info;
  int_info.value = read<Vmx::Vmcs_vm_entry_interrupt_info>();

  if (int_info.valid())
    {
      // Do event injection

      // Load error code, if required
      if (int_info.deliver_error_code())
        to_vmcs<Vmx::Vmcs_vm_entry_exception_error>();

      // Interrupt types 4 (software interrupt),
      // 5 (privileged software exception) and 6 (software exception)
      // require the instruction length field.
      //
      // All these interrupt types have the bit 10 set.
      if (int_info.deliver_insn_length())
        to_vmcs<Vmx::Vmcs_vm_entry_insn_len>();

      Vmx::vmcs_write<Vmx::Vmcs_vm_entry_interrupt_info>(int_info.value);
    }

  // hm, we have to check for sanitizing the cr0 and cr4 shadow stuff
  to_vmcs<Vmx::Vmcs_cr0_guest_host_mask>();
  to_vmcs<Vmx::Vmcs_cr4_guest_host_mask>();
  to_vmcs<Vmx::Vmcs_cr0_read_shadow>();
  to_vmcs<Vmx::Vmcs_cr4_read_shadow>();

  // no cr3 target values supported
}

/**
 * Move the guest CR3 from software VMCS to hardware VMCS.
 */
PUBLIC inline
template<Host_state HOST_STATE>
void
Vmx_vm_state_t<HOST_STATE>::load_cr3()
{
  to_vmcs<Vmx::Vmcs_guest_cr3>();
}

/**
 * Move the guest state from hardware VMCS to software VMCS.
 */
PUBLIC inline
template<Host_state HOST_STATE>
void
Vmx_vm_state_t<HOST_STATE>::store_guest_state()
{
  Cpu_number const cpu = current_cpu();
  Vmx &vmx = Vmx::cpus.cpu(cpu);

  // read 16-bit fields
  from_vmcs<Vmx::Vmcs_guest_es_selector>();
  from_vmcs<Vmx::Vmcs_guest_cs_selector>();
  from_vmcs<Vmx::Vmcs_guest_ss_selector>();
  from_vmcs<Vmx::Vmcs_guest_ds_selector>();
  from_vmcs<Vmx::Vmcs_guest_fs_selector>();
  from_vmcs<Vmx::Vmcs_guest_gs_selector>();
  from_vmcs<Vmx::Vmcs_guest_ldtr_selector>();
  from_vmcs<Vmx::Vmcs_guest_tr_selector>();
  from_vmcs<Vmx::Vmcs_guest_interrupt_status>();

  // read 64-bit fields
  from_vmcs<Vmx::Vmcs_guest_ia32_debugctl>();

  Vmx_info::Flags<Unsigned32> exit_ctls
    = Vmx_info::Flags<Unsigned32>(
        vmx.info.exit_ctls.apply(read<Vmx::Vmcs_vm_exit_ctls>()));

  if (exit_ctls.test(Vmx_info::Ex_save_ia32_pat))
    from_vmcs<Vmx::Vmcs_guest_ia32_pat>();
  if (exit_ctls.test(Vmx_info::Ex_save_ia32_efer))
    from_vmcs<Vmx::Vmcs_guest_ia32_efer>();
  if (exit_ctls.test(Vmx_info::Ex_save_preemption_timer))
    from_vmcs<Vmx::Vmcs_preemption_timer_value>();

  // read 32-bit fields
  from_vmcs<Vmx::Vmcs_guest_es_limit>();
  from_vmcs<Vmx::Vmcs_guest_cs_limit>();
  from_vmcs<Vmx::Vmcs_guest_ss_limit>();
  from_vmcs<Vmx::Vmcs_guest_ds_limit>();
  from_vmcs<Vmx::Vmcs_guest_fs_limit>();
  from_vmcs<Vmx::Vmcs_guest_gs_limit>();
  from_vmcs<Vmx::Vmcs_guest_ldtr_limit>();
  from_vmcs<Vmx::Vmcs_guest_tr_limit>();
  from_vmcs<Vmx::Vmcs_guest_gdtr_limit>();
  from_vmcs<Vmx::Vmcs_guest_idtr_limit>();
  from_vmcs<Vmx::Vmcs_guest_es_access_rights>();
  from_vmcs<Vmx::Vmcs_guest_cs_access_rights>();
  from_vmcs<Vmx::Vmcs_guest_ss_access_rights>();
  from_vmcs<Vmx::Vmcs_guest_ds_access_rights>();
  from_vmcs<Vmx::Vmcs_guest_fs_access_rights>();
  from_vmcs<Vmx::Vmcs_guest_gs_access_rights>();
  from_vmcs<Vmx::Vmcs_guest_ldtr_access_rights>();
  from_vmcs<Vmx::Vmcs_guest_tr_access_rights>();
  from_vmcs<Vmx::Vmcs_guest_interruptibility_state>();
  from_vmcs<Vmx::Vmcs_guest_activity_state>();

  // sysenter msr is not saved here, because we trap all msr accesses right now
  if (0)
    {
      from_vmcs<Vmx::Vmcs_guest_ia32_sysenter_cs>();
      from_vmcs<Vmx::Vmcs_guest_ia32_sysenter_esp>();
      from_vmcs<Vmx::Vmcs_guest_ia32_sysenter_eip>();
    }

  // read natural-width fields
  from_vmcs<Vmx::Vmcs_guest_cr0>();
  from_vmcs<Vmx::Vmcs_guest_cr4>();
  from_vmcs<Vmx::Vmcs_guest_es_base>();
  from_vmcs<Vmx::Vmcs_guest_cs_base>();
  from_vmcs<Vmx::Vmcs_guest_ss_base>();
  from_vmcs<Vmx::Vmcs_guest_ds_base>();
  from_vmcs<Vmx::Vmcs_guest_fs_base>();
  from_vmcs<Vmx::Vmcs_guest_gs_base>();
  from_vmcs<Vmx::Vmcs_guest_ldtr_base>();
  from_vmcs<Vmx::Vmcs_guest_tr_base>();
  from_vmcs<Vmx::Vmcs_guest_gdtr_base>();
  from_vmcs<Vmx::Vmcs_guest_idtr_base>();
  from_vmcs<Vmx::Vmcs_guest_dr7>();
  from_vmcs<Vmx::Vmcs_guest_rsp>();
  from_vmcs<Vmx::Vmcs_guest_rip>();
  from_vmcs<Vmx::Vmcs_guest_rflags>();
  from_vmcs<Vmx::Vmcs_guest_pending_dbg_exceptions>();
}

/**
 * Store exit information to software VMCS.
 *
 * \param error   Instruction error.
 * \param reason  Exit reason.
 */
PUBLIC inline
template<Host_state HOST_STATE>
void
Vmx_vm_state_t<HOST_STATE>::store_exit_info(Unsigned32 error,
                                            Unsigned32 reason)
{
  // Clear the valid bit in VM-entry interruption information
  Vmx_vm_entry_interrupt_info int_info;
  int_info.value = read<Vmx::Vmcs_vm_entry_interrupt_info>();

  if (int_info.valid())
    {
      int_info.valid() = 0;
      write<Vmx::Vmcs_vm_entry_interrupt_info>(int_info.value);
    }

  // read 32-bit fields
  write<Vmx::Vmcs_vm_insn_error>(error);
  write<Vmx::Vmcs_exit_reason>(reason);
  from_vmcs<Vmx::Vmcs_vm_exit_interrupt_info>();
  from_vmcs<Vmx::Vmcs_vm_exit_interrupt_error>();
  from_vmcs<Vmx::Vmcs_idt_vectoring_info>();
  from_vmcs<Vmx::Vmcs_idt_vectoring_error>();
  from_vmcs<Vmx::Vmcs_vm_exit_insn_length>();
  from_vmcs<Vmx::Vmcs_vm_exit_insn_info>();

  // read natural-width fields
  from_vmcs<Vmx::Vmcs_exit_qualification>();
  from_vmcs<Vmx::Vmcs_io_rcx>();
  from_vmcs<Vmx::Vmcs_io_rsi>();
  from_vmcs<Vmx::Vmcs_io_rdi>();
  from_vmcs<Vmx::Vmcs_io_rip>();
  from_vmcs<Vmx::Vmcs_guest_linear_address>();
}

/**
 * Move the guest CR3 from hardware VMCS to software VMCS.
 */
PUBLIC inline
template<Host_state HOST_STATE>
void
Vmx_vm_state_t<HOST_STATE>::store_cr3()
{
  from_vmcs<Vmx::Vmcs_guest_cr3>();
}

/**
 * Move the guest physical address from hardware VMCS to software VMCS.
 */
PUBLIC inline
template<Host_state HOST_STATE>
void
Vmx_vm_state_t<HOST_STATE>::store_guest_physical_address()
{
  from_vmcs<Vmx::Vmcs_guest_physical_address>();
}

/**
 * Setup the hardware VMCS of a context.
 *
 * This method follows a similar logic as implemented in \ref
 * Thread_object::sys_vcpu_resume() for the user_task member.
 *
 * To avoid the constly capability lookup, the capability in the \ref _vmcs
 * member is examined only if it contains no operation. If the capability
 * refers to a valid VMCS object, it is then set as the hardware VMCS of the
 * given context. The capability in \ref _vmcs is then marked with the
 * \ref L4_obj_ref::Ipc_send operation.
 *
 * If the \ref _vmcs member contains the \ref L4_obj_ref::Ipc_reply operation,
 * the hardware VMCS of the given context is set to none.
 *
 * \param ctxt  Context for which to set the hardware VMCS.
 *
 * \retval 0               Hardware VMCS setup successfully.
 * \retval -L4_err::EPerm  Capability with invalid rights.
 */
PUBLIC inline NEEDS[Vmx_vm_state_t::clear_vmcs, Vmx_vm_state_t::replace_vmcs]
template<Host_state HOST_STATE>
int
Vmx_vm_state_t<HOST_STATE>::setup_vmcs(Context *ctxt)
{
  if (_vmcs.valid() && _vmcs.op() == 0)
    {
      Space *space = ctxt->space();

      L4_fpage::Rights rights = L4_fpage::Rights(0);
      Vmx_vmcs *vmx_vmcs
        = cxx::dyn_cast<Vmx_vmcs *>(space->lookup_local(_vmcs.cap(), &rights));

      if (EXPECT_FALSE(vmx_vmcs && !(rights & L4_fpage::Rights::CS())))
        return -L4_err::EPerm;

      Vmx_vmcs *prev = ctxt->vmcs();
      if (vmx_vmcs != prev)
        {
          if (!replace_vmcs(ctxt, prev, vmx_vmcs))
            return -L4_err::EBusy;
        }

      reinterpret_cast<Mword &>(_vmcs) |= L4_obj_ref::Ipc_send;
    }
  else if (_vmcs.op() == L4_obj_ref::Ipc_reply)
    clear_vmcs(ctxt);

  return 0;
}

/**
 * Set the hardware VMCS of a context to none.
 *
 * \param ctxt  Context for which to set the hardware VMCS to none.
 */
PUBLIC inline NEEDS[Vmx_vm_state_t::replace_vmcs]
template<Host_state HOST_STATE>
void
Vmx_vm_state_t<HOST_STATE>::clear_vmcs(Context *ctxt)
{
  Vmx_vmcs *prev = ctxt->vmcs();
  replace_vmcs(ctxt, prev, nullptr);
}

/**
 * Replace the hardware VMCS of the given context.
 *
 * Manage the reference counting of the previous and the next VMCS object.
 *
 * \note It is essential that the reference count equals exactly to the number
 *       of persistent references of the VMCS object. The correctness of the
 *       VMCS region clearing and VMCS object deletion is determined by the
 *       reference count dropping to zero.
 *
 * \param ctxt  Context for which to replace the hardware VMCS.
 * \param prev  Previous hardware VMCS (can be nullptr).
 * \param next  Next hardware VMCS to set (can be nullptr).
 *
 * \retval true   The replacement was successful.
 * \retval false  The replacement failed.
 */
PRIVATE inline
template<Host_state HOST_STATE>
bool
Vmx_vm_state_t<HOST_STATE>::replace_vmcs(Context *ctxt, Vmx_vmcs *prev,
                                         Vmx_vmcs *next)
{
  // Bind the next VMCS object.
  if (next)
    {
      // Make sure the hardware VMCS is not already bound.
      if (!next->set_owner(this))
        return false;

      next->inc_ref();
    }

  ctxt->set_vmcs(next);

  // Unbind the previous VMCS object.
  if (prev)
    {
      prev->clear_owner();

      if (!prev->dec_ref())
        delete prev;
    }

  return true;
}

PRIVATE
bool
Vmx::handle_bios_lock()
{
  enum
  {
    Feature_control_lock            = 1 << 0,
    Feature_control_vmx_outside_SMX = 1 << 2,
  };

  Unsigned64 feature = Cpu::rdmsr(Msr::Ia32_feature_control);

  if (feature & Feature_control_lock)
    {
      if (!(feature & Feature_control_vmx_outside_SMX))
        return false;
    }
  else
    Cpu::wrmsr(feature | Feature_control_vmx_outside_SMX | Feature_control_lock,
               Msr::Ia32_feature_control);
  return true;
}

/**
 * Execute the VMXON instruction.
 *
 * \param vmxon_pa  VMXON physical address argument.
 *
 * \retval true   Instruction executed successfully.
 * \retval false  Instruction failed.
 */
PRIVATE static inline
bool
Vmx::vmxon(Unsigned64 vmxon_pa)
{
  Mword flags;

  asm volatile (
    "vmxon %[vmxon_pa]\n\t"
    "pushf\n\t"
    "pop %[flags]\n\t"
    : [flags] "=r" (flags)
    : [vmxon_pa] "m" (vmxon_pa)
    : "cc"
  );

  return (flags & 0x41) == 0;
}

/**
 * Execute the VMCLEAR instruction.
 *
 * \param vmcs_pa  VMCS physical address argument.
 *
 * \retval true   Instruction executed successfully.
 * \retval false  Instruction failed.
 */
PRIVATE static inline
bool
Vmx::vmclear(Unsigned64 vmcs_pa)
{
  Mword flags;

  asm volatile (
    "vmclear %[vmcs_pa]\n\t"
    "pushf\n\t"
    "pop %[flags]\n\t"
    : [flags] "=r" (flags)
    : [vmcs_pa] "m" (vmcs_pa)
    : "cc"
  );

  return (flags & 0x41) == 0;
}

/**
 * Execute the VMPTRLD instruction.
 *
 * \param vmcs_pa  VMCS physical address argument.
 *
 * \retval true   Instruction executed successfully.
 * \retval false  Instruction failed.
 */
PRIVATE static inline
bool
Vmx::vmptrld(Unsigned64 vmcs_pa)
{
  Mword flags;

  asm volatile (
    "vmptrld %[vmcs_pa]\n\t"
    "pushf\n\t"
    "pop %[flags]\n\t"
    : [flags] "=r" (flags)
    : [vmcs_pa] "m" (vmcs_pa)
    : "cc"
  );

  return (flags & 0x41) == 0;
}

PUBLIC
void
Vmx::pm_on_resume(Cpu_number) override
{
  check(handle_bios_lock());
  if (!vmxon(_vmxon_pa))
    _vmx_enabled = false;
}

PUBLIC
void
Vmx::pm_on_suspend(Cpu_number) override
{
  clear_vmx_vmcs(_current_vmcs);
}

PUBLIC
Vmx::Vmx(Cpu_number cpu)
  : _vmx_enabled(false), _has_vpid(false), _current_vmcs(nullptr)
{
  Cpu &c = Cpu::cpus.cpu(cpu);

  if (cpu == Cpu::invalid() || !c.vmx())
    {
      if (cpu == Cpu_number::boot_cpu())
        WARNX(Info, "VMX: Not supported\n");
      return;
    }

  // check whether vmx is enabled by BIOS
  if (!handle_bios_lock())
    {
      if (cpu == Cpu_number::boot_cpu())
        WARNX(Info, "VMX: CPU has VMX support but it is disabled\n");
      return;
    }

  if (!info.init(cpu))
    {
      if (cpu == Cpu_number::boot_cpu())
        printf("VMX: Disabling due to lack of features\n");

      return;
    }

  // check for vpid support
  if (info.procbased_ctls2.allowed(Vmx_info::PRB2_enable_vpid))
    _has_vpid = true;

  c.set_cr4(c.get_cr4() | (1 << 13)); // set CR4.VMXE to 1

  // if NE bit is not set vmxon will fail
  c.set_cr0(c.get_cr0() | (1 << 5));

  Unsigned32 vmcs_size = ((info.basic & (0x1fffULL << 32)) >> 32);
  if (vmcs_size > Vmx_vmcs::Vmcs_size)
    {
      if (cpu == Cpu_number::boot_cpu())
        printf("VMX: VMCS size of %u bytes not supported\n", vmcs_size);

      return;
    }

  // allocate a 4 KiB aligned region for VMXON
  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  check(_vmxon = Kmem_alloc::allocator()->alloc(Order(12)));

  _vmxon_pa = Kmem::virt_to_phys(_vmxon);

  // init vmxon region with vmcs revision identifier
  // which is stored in the lower 32 bits of MSR 0x480
  *static_cast<Unsigned32 *>(_vmxon) = info.basic & 0x7fffffffU;

  // Enable VMX operation
  if (vmxon(_vmxon_pa))
    {
      _vmx_enabled = true;

      if (cpu == Cpu_number::boot_cpu())
        printf("VMX: initialized\n");

      Pm_object::register_pm(cpu);
    }
  else
    {
      if (cpu == Cpu_number::boot_cpu())
        printf("VMX: initialization failed\n");
    }
}

/**
 * Initialize the extended vCPU state.
 *
 * Initialize the Vmx_user_info part of the extended vCPU state.
 *
 * \param vcpu_state  vCPU state to initialize.
 */
PUBLIC inline
void
Vmx::init_vcpu_state(void *vcpu_state)
{
  Cpu &cpu = Cpu::cpus.cpu(current_cpu());

  if (cpu.features() & FEAT_PAT
      && info.exit_ctls.allowed(Vmx_info::Ex_load_ia32_pat))
    {
      info.exit_ctls.enforce(Vmx_info::Ex_load_ia32_pat, true);
    }
  else
    {
      // We have no proper PAT support, so disallow PAT load store for
      // guest too
      info.exit_ctls.enforce(Vmx_info::Ex_save_ia32_pat, false);
      info.entry_ctls.enforce(Vmx_info::En_load_ia32_pat, false);
    }

  if (info.exit_ctls.allowed(Vmx_info::Ex_load_ia32_efer))
    {
      info.exit_ctls.enforce(Vmx_info::Ex_load_ia32_efer, true);
    }
  else
    {
      // We have no EFER load for host, so disallow EFER load store for
      // guest too
      info.exit_ctls.enforce(Vmx_info::Ex_save_ia32_efer, false);
      info.entry_ctls.enforce(Vmx_info::En_load_ia32_efer, false);
    }

  if (!info.exit_ctls.allowed(Vmx_info::Ex_load_perf_global_ctl))
    // do not allow Load IA32_PERF_GLOBAL_CTRL on entry
    info.entry_ctls.enforce(Vmx_info::En_load_perf_global_ctl, false);

  Vmx_user_info *i
    = offset_cast<Vmx_user_info *>(vcpu_state, Config::Ext_vcpu_infos_offset);
  i->basic = info.basic;
  i->pinbased = info.pinbased_ctls;
  i->procbased = info.procbased_ctls;
  i->exit = info.exit_ctls;
  // relax the IRQ on exit for the VMM. Nevertheless,
  // our API hides this feature completely from the VMM
  i->exit.relax(Vmx_info::Ex_ack_irq_on_exit);
  i->entry = info.entry_ctls;
  i->misc = info.misc;
  i->cr0_or = info.cr0_defs.must_be_one();
  i->cr0_and = info.cr0_defs.may_be_one();
  i->cr4_or = info.cr4_defs.must_be_one();
  i->cr4_and = info.cr4_defs.may_be_one();
  i->vmcs_field_info = info.max_index;
  i->procbased2 = info.procbased_ctls2;
  i->ept_vpid_cap = info.ept_vpid_cap;

  // Nested virtualization not implemented.
  i->nested_revision = 0;

  i->pinbased_dfl1 = info.pinbased_ctls_default1;
  i->procbased_dfl1 = info.procbased_ctls_default1;
  i->exit_dfl1 = info.exit_ctls_default1;
  i->entry_dfl1 = info.entry_ctls_default1;

  Vmx_vm_state *s
    = offset_cast<Vmx_vm_state *>(vcpu_state, Config::Ext_vcpu_state_offset);
  s->init();
}

/**
 * Load the VMCS object as the current hardware VMCS.
 *
 * If the VMCS object is already the current hardware VMCS, then no operation
 * is performed. If the VMCS memory region has been reset, it is cleared before
 * loading and then initialized.
 *
 * If the VMCS object is nullptr, its existence lock is invalid or if it has
 * been loaded to a different CPU already, the operation fails.
 *
 * \param vmx_vmcs  VMCS object to load.
 *
 * \retval 0                VMCS object loaded successfully.
 * \retval -L4_err::EInval  Invalid VMCS object.
 * \retval -L4_err::EBusy   VMCS object already loaded to a different CPU.
 * \retval -L4_err::ENodev  VMPTRLD instruction failed.
 */
PUBLIC inline NEEDS[Vmx::initialize_current_vmcs, Vmx::vmclear, Vmx::vmptrld]
int
Vmx::load_vmx_vmcs(Vmx_vmcs *vmx_vmcs)
{
  if (EXPECT_FALSE(!vmx_vmcs))
    return -L4_err::EInval;

  // When the existence lock of an VMCS object is invalid, the VMCS region is
  // about to be cleared and eventually removed. Therefore we cannot make
  // the VMCS region active again, as that would break the correctness of the
  // VMCS object deletion.
  if (EXPECT_FALSE(!vmx_vmcs->existence_lock.valid()))
    return -L4_err::EInval;

  Cpu_number active = vmx_vmcs->active();
  Cpu_number cpu = current_cpu();

  if (EXPECT_FALSE(active != Cpu_number::nil() && active != cpu))
    return -L4_err::EBusy;

  if (_current_vmcs != vmx_vmcs)
    {
      void *vmcs = vmx_vmcs->ptr();
      Address vmcs_pa = Kmem::virt_to_phys(vmcs);

      Unsigned32 *revision = reinterpret_cast<Unsigned32 *>(vmcs);
      bool initialized = *revision != 0;

      // Clear the VMCS before loading for the first time.
      if (EXPECT_FALSE(!initialized))
        {
          *revision = info.basic & 0x7fffffffU;
          vmclear(vmcs_pa);
        }

      if (EXPECT_FALSE(!vmptrld(vmcs_pa)))
        return -L4_err::ENodev;

      vmx_vmcs->set_active(cpu);
      _current_vmcs = vmx_vmcs;

      // Initialize the VMCS which has been loaded for the first time.
      if (EXPECT_FALSE(!initialized))
        {
          initialize_current_vmcs();
          if (EXPECT_FALSE(vmwrite_failure.current()))
            return -L4_err::ENodev;
        }
    }

  return 0;
}

/**
 * Clear the VMCS object.
 *
 * If the cleared VMCS object is the current hardware VMCS, then the current
 * hardware VMCS is set to none.
 *
 * If the VMCS object is nullptr or if it has been loaded to a different CPU,
 * the operation fails.
 *
 * \param vmx_vmcs  VMCS object to clear. In case it is nullptr, then no
 *                  operation is performed.
 *
 * \retval 0                VMCS object cleared successfully.
 * \retval -L4_err::EInval  Invalid VMCS object.
 * \retval -L4_err::EBusy   VMCS object loaded to a different CPU.
 */
PUBLIC inline NEEDS[Vmx::vmclear]
int
Vmx::clear_vmx_vmcs(Vmx_vmcs *vmx_vmcs)
{
  if (!vmx_vmcs)
    return -L4_err::EInval;

  Cpu_number active = vmx_vmcs->active();
  Cpu_number cpu = current_cpu();

  if (active != Cpu_number::nil() && active != cpu)
    return -L4_err::EBusy;

  void *vmcs = vmx_vmcs->ptr();
  Address vmcs_pa = Kmem::virt_to_phys(vmcs);

  vmx_vmcs->clear_active();
  vmclear(vmcs_pa);

  if (vmx_vmcs == _current_vmcs)
    _current_vmcs = nullptr;

  return 0;
}

PUBLIC inline
bool
Vmx::vmx_enabled() const
{ return _vmx_enabled; }

PUBLIC inline
bool
Vmx::has_vpid() const
{ return _has_vpid; }

/**
 * Get VMWRITE failure flag on the current CPU.
 *
 * \retval false  No VMWRITE instruction failed on the current CPU.
 * \reval  true   A VMWRITE instruction failed on the current CPU.
 */
PUBLIC static inline
bool
Vmx::vmx_failure()
{ return vmwrite_failure.current(); }

/**
 * Slab allocator for the VMCS objects.
 */
static Kmem_slab_t<Vmx_vmcs> _vmx_vmcs_allocator("VMX VMCS");

/**
 * VMCS object constructor.
 *
 * Initialize the reference counting and reset the contents of the VMCS memory
 * region.
 *
 * \param quota  RAM quota of this object.
 * \param addr   Pre-allocated, page-aligned VMCS memory region.
 */
PUBLIC
Vmx_vmcs::Vmx_vmcs(Ram_quota *quota, void *addr)
  : _ptr(addr),
    _quota(quota),
    _owner(nullptr),
    _active(Cpu_number::nil())
{
  assert(_ptr);

  memset(_ptr, 0, Vmcs_size);
  inc_ref(true);
}

/**
 * Clear the VMCS object as the final phase of VMCS object deletion.
 *
 * The VMCS object needs to be cleared prior to deletion to make sure that
 * the CPU stops accessing the VMCS region before the physical memory is
 * recycled.
 *
 * The VMCS region can be safely cleared if there is no possibility that the
 * VMCS region is or will become active on any CPU.
 *
 * When this method is called from \ref Kobject::Reap_list::del_2(), there is
 * no longer any capability reference to this VMCS object and therefore this
 * object will no longer be bound to a vCPU in \ref Vmx_vm_state::setup_vmcs().
 *
 * Furthermore, when this method is called, the existence lock of this VMCS
 * object is already invalid, thus the VMCS region can no longer become active
 * in \ref Vmx::load_vmx_vmcs() even if the VMCS object is still bound to a
 * vCPU.
 *
 * Finally, since this method is called after an RCU grace period, there are
 * no longer any ephemeral references to this VMCS object. In other words,
 * any vCPU that might have been running with this VMCS region as active has
 * already exited.
 *
 * \retval true   The object is not referenced anymore and it is ready to be
 *                deleted.
 * \retval false  The object is still referenced.
 */
PUBLIC virtual
bool
Vmx_vmcs::put() override
{
  shootdown();
  return dec_ref() == 0;
}

/**
 * Clear the VMCS object on the CPU where it is active.
 *
 * The VMCS object needs to be cleared prior to deletion to make sure that
 * the CPU stops accessing the VMCS region before the physical memory is
 * recycled.
 *
 * \note This method might be called from a different CPU than the CPU where
 *       the VMCS region is active. Therefore we have to use a remote CPU call
 *       to clear the VMCS on the proper CPU. If the active CPU is the current
 *       CPU, then the remote call is automatically optimized to a local call.
 */
PRIVATE inline
void
Vmx_vmcs::shootdown()
{
  if (_active == Cpu_number::nil())
    return;

  Cpu_mask active_cpu;
  active_cpu.set(_active);

  Cpu_call::cpu_call_many(active_cpu, [this](Cpu_number)
    {
      Vmx &vmx = Vmx::cpus.current();
      vmx.clear_vmx_vmcs(this);
      return false;
    });
}

/**
 * VMCS object destructor.
 *
 * The VMCS region is deallocated.
 */
PUBLIC
Vmx_vmcs::~Vmx_vmcs()
{
  assert(_ptr);
  Kmem_alloc::allocator()->q_free(_quota, Order(Vmcs_order), _ptr);
}

/**
 * Allocate a new VMCS object.
 *
 * The \ref _vmx_vmcs_allocator slab allocator is used.
 *
 * \param size   Size of the object.
 * \param quota  RAM quota of the object.
 *
 * \return Pointer to the allocated object.
 */
PUBLIC static
void *
Vmx_vmcs::operator new([[maybe_unused]] size_t size, Ram_quota *quota) noexcept
{
  assert(size == sizeof(Vmx_vmcs));
  return _vmx_vmcs_allocator.q_alloc(quota);
}

/**
 * Delete a VMCS object.
 *
 * The \ref _vmx_vmcs_allocator slab allocator is used.
 *
 * The VMCS object can be safely deleted if the following conditions are met:
 *
 * (1) There are no persistent references to the VMCS object.
 * (2) There are no ephemeral references to the VMCS object (besides the
 *     ephemeral reference that is calling the delete operator).
 * (3) The VMCS region has been cleared.
 *
 * The delete operator is only ever called after the reference count of the
 * VMCS object has dropped to zero. This guarantees that the condition (1) is
 * met.
 *
 * If the delete operator is called from \ref Kobject::Reap_list::del_2(),
 * an RCU grace period has passed which furthermore guarantees that there are
 * no longer any ephemeral references to this VMCS object and that the VMCS
 * region has been cleared in the prior call of \ref Vmx_vmcs::put().
 *
 * If the delete operator is called from \ref Vmx_vm_state::replace_vmcs(),
 * there is no longer any capability reference that would spawn any persistent
 * reference and the \ref Vmx_vmcs::put() method has been already called after
 * an RCU grace period in the process of the capability reference removal.
 *
 * In both cases, it is guaranteed that the conditions (2) and (3) are met.
 *
 * \param ptr  VMCS object to be deleted.
 */
PUBLIC static
void
Vmx_vmcs::operator delete(Vmx_vmcs *vmcs, std::destroying_delete_t)
{
  Ram_quota *q = vmcs->ram_quota();
  vmcs->~Vmx_vmcs();
  _vmx_vmcs_allocator.q_free(q, vmcs);
}

/**
 * Get the VMCS memory region of the VMCS object.
 *
 * \return VMCS memory region.
 */
PUBLIC inline
void *
Vmx_vmcs::ptr() const
{
  return _ptr;
}

/**
 * Get the RAM quota of the VMCS object.
 *
 * \return RAM quota.
 */
PUBLIC inline
Ram_quota *
Vmx_vmcs::ram_quota() const
{
  return _quota;
}

/**
 * Get the CPU where the VMCS object is active.
 *
 * \return CPU where the VMCS object is active.
 */
PUBLIC inline
Cpu_number
Vmx_vmcs::active() const
{
  return _active;
}

/**
 * Atomically set the extended vCPU state to which the VMCS object is bound to.
 *
 * \param owner  Extended vCPU state to which the VMCS object is bound to.
 *
 * \retval true   The VMCS object has been bound to the extended vCPU state.
 * \retval false  The VMCS object is already bound to a different vCPU state.
 *
 */
PUBLIC inline
bool
Vmx_vmcs::set_owner(Vmx_vm_state *owner)
{
  assert(owner);
  return cas<Vmx_vm_state *>(&_owner, nullptr, owner);
}

/**
 * Set the CPU where the VMCS object is active.
 *
 * \param active  CPU where the VMCS object is active.
 */
PUBLIC inline
void
Vmx_vmcs::set_active(Cpu_number active)
{
  _active = active;
}

/**
 * Mark the VMCS object as unbound to any extended vCPU state.
 */
PUBLIC inline
void
Vmx_vmcs::clear_owner()
{
  _owner = nullptr;
}

/**
 * Mark the VMCS object inactive on any CPU.
 */
PUBLIC inline
void
Vmx_vmcs::clear_active()
{
  _active = Cpu_number::nil();
}

/**
 * Invoke an operation on the VMCS object.
 *
 * Currently, the VMCS object defines no operations.
 *
 * \param Syscall frame of the invocation.
 */
PUBLIC
void
Vmx_vmcs::invoke(L4_obj_ref, L4_fpage::Rights, Syscall_frame *frame,
                 Utcb *) override
{
  if (EXPECT_FALSE(frame->tag().proto() != L4_msg_tag::Label_vcpu_context))
    {
      frame->tag(commit_result(-L4_err::EBadproto));
      return;
    }

  frame->tag(commit_result(-L4_err::ENosys));
}

namespace {

/**
 * Factory for creating VMCS objects.
 *
 * Before the VMCS object is allocated, the VMCS memory region associated with
 * it is allocated.
 *
 * \param      quota  RAM quota.
 * \param[out] err    Error code in case of allocation failure.
 *
 * \return VMCS object or nullptr if memory allocation failed.
 */
static Kobject_iface * FIASCO_FLATTEN
vmx_vmcs_factory(Ram_quota *quota, Space *, L4_msg_tag, Utcb const *, Utcb *,
                 int *err, unsigned *)
{
  *err = L4_err::ENomem;

  void *vmcs
    = Kmem_alloc::allocator()->q_alloc(quota, Order(Vmx_vmcs::Vmcs_order));
  if (!vmcs)
    return nullptr;

  return new (quota) Vmx_vmcs(quota, vmcs);
}

/**
 * VMCS object factory registration.
 */
static inline
void __attribute__((constructor)) FIASCO_INIT_SFX(vmx_vmcs_register_factory)
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_vcpu_context, vmx_vmcs_factory);
}

} // namespace
