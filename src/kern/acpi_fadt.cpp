INTERFACE:

#include <acpi.h>

class Acpi_fadt : public Acpi_table_head
{
public:
  Unsigned32 facs_addr;
  Unsigned32 dsdt_addr;
  Unsigned8  _res0;
  enum Pm_profile : Unsigned8
  {
    Pm_unspec             = 0,
    Pm_desktop            = 1,
    Pm_mobile             = 2,
    Pm_workstation        = 3,
    Pm_enterprise_server  = 4,
    Pm_soho_server        = 5,
    Pm_appliance_pc       = 6,
    Pm_performance_server = 7,
    Pm_tablet             = 8,
  };
  Pm_profile pm_profile;
  Unsigned16 sci_int;
  Unsigned32 smi_cmd_port;
  Unsigned8  acpi_enable;
  Unsigned8  acpi_disable;
  Unsigned8  s4bios_req;
  Unsigned8  pstate_control;
  Unsigned32 pm1a_evt_blk;
  Unsigned32 pm1b_evt_blk;
  Unsigned32 pm1a_cntl_blk;
  Unsigned32 pm1b_cntl_blk;
  Unsigned32 pm2_cntl_blk;
  Unsigned32 pm_tmr_blk;
  Unsigned32 gpe0_blk;
  Unsigned32 gpe1_blk;
  Unsigned8  pm1_evt_len;
  Unsigned8  pm1_cntl_len;
  Unsigned8  pm2_cntl_len;
  Unsigned8  pm_tmr_len;
  Unsigned8  gpe0_blk_len;
  Unsigned8  gpe1_blk_len;
  Unsigned8  gpe1_base;
  Unsigned8  cst_cntl;
  Unsigned16 p_lvl2_lat;
  Unsigned16 p_lvl3_lat;
  Unsigned16 flush_size;
  Unsigned16 flush_stride;
  Unsigned8  duty_offset;
  Unsigned8  duty_width;
  Unsigned8  day_alarm;
  Unsigned8  mon_alarm;
  Unsigned8  century;
  Unsigned16 iapc_boot_arch;
  Unsigned8  _res1;
  Unsigned32 flags;
  Acpi_gas   reset_regs;
  Unsigned8  reset_value;
  Unsigned8  _res2[3];
  Unsigned64 x_facs_addr;
  Unsigned64 x_dsdt_addr;
  Acpi_gas   x_pm1a_evt_blk;
  Acpi_gas   x_pm1b_evt_blk;
  Acpi_gas   x_pm1a_cntl_blk;
  Acpi_gas   x_pm1b_cntl_blk;
  Acpi_gas   x_pm2_cntl_blk;
  Acpi_gas   x_pm_tmr_blk;
  Acpi_gas   x_gpe0_blk;
  Acpi_gas   x_gpe1_blk;
  Acpi_gas   sleep_cntl_reg;
  Acpi_gas   sleep_status_reg;
} __attribute__((packed));

class Acpi_facs
{
public:
  char       signature[4];
  Unsigned32 len;
  Unsigned32 hw_signature;
  Unsigned32 fw_wake_vector;
  Unsigned32 global_lock;
  Unsigned32 flags;
  Unsigned64 x_fw_wake_vector;
  Unsigned8  version;
  Unsigned8  _res0[3];
  Unsigned32 ospm_flags;
  Unsigned32 _res1[6];
  Unsigned8  f_s4bios;
  Unsigned8  f_64bit_wake_supported;
  Unsigned8  _res2[30];
  Unsigned8  f_64bit_wake;
  Unsigned8  _res3[31];
} __attribute__((packed));
