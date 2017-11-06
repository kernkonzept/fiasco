INTERFACE[ia32,amd64,ux]:

#include "types.h"
#include "initcalls.h"
#include "regdefs.h"
#include "per_cpu_data.h"

EXTENSION
class Cpu
{
public:

  enum Vendor
  {
    Vendor_unknown = 0,
    Vendor_intel,
    Vendor_amd,
    Vendor_cyrix,
    Vendor_via,
    Vendor_umc,
    Vendor_nexgen,
    Vendor_rise,
    Vendor_transmeta,
    Vendor_sis,
    Vendor_nsc
  };

  enum CacheTLB
  {
    Cache_unknown = 0,
    Cache_l1_data,
    Cache_l1_inst,
    Cache_l1_trace,
    Cache_l2,
    Cache_l3,
    Tlb_data_4k,
    Tlb_inst_4k,
    Tlb_data_4M,
    Tlb_inst_4M,
    Tlb_data_4k_4M,
    Tlb_inst_4k_4M,
    Tlb_data_2M_4M,
  };

  enum
  {
    Ldt_entry_size = 8,
  };

  enum Local_features
  {
    Lf_rdpmc		= 0x00000001,
    Lf_rdpmc32		= 0x00000002,
  };

  Unsigned64 time_us() const;
  int can_wrmsr() const;

private:
  void init();
  Unsigned64 _frequency;
  Unsigned32 _version;
  Unsigned32 _brand;
  Unsigned32 _features;
  Unsigned32 _ext_features;
  Unsigned32 _ext_07_ebx;
  Unsigned32 _ext_8000_0001_ecx;
  Unsigned32 _ext_8000_0001_edx;
  Unsigned32 _local_features;

  Unsigned16 _inst_tlb_4k_entries;
  Unsigned16 _data_tlb_4k_entries;
  Unsigned16 _inst_tlb_4m_entries;
  Unsigned16 _data_tlb_4m_entries;
  Unsigned16 _inst_tlb_4k_4m_entries;
  Unsigned16 _data_tlb_4k_4m_entries;
  Unsigned16 _l2_inst_tlb_4k_entries;
  Unsigned16 _l2_data_tlb_4k_entries;
  Unsigned16 _l2_inst_tlb_4m_entries;
  Unsigned16 _l2_data_tlb_4m_entries;

  Unsigned16 _l1_trace_cache_size;
  Unsigned16 _l1_trace_cache_asso;

  Unsigned16 _l1_data_cache_size;
  Unsigned16 _l1_data_cache_asso;
  Unsigned16 _l1_data_cache_line_size;

  Unsigned16 _l1_inst_cache_size;
  Unsigned16 _l1_inst_cache_asso;
  Unsigned16 _l1_inst_cache_line_size;

  Unsigned16 _l2_cache_size;
  Unsigned16 _l2_cache_asso;
  Unsigned16 _l2_cache_line_size;

  Unsigned32 _l3_cache_size;
  Unsigned16 _l3_cache_asso;
  Unsigned16 _l3_cache_line_size;

  Unsigned8 _phys_bits;
  Unsigned8 _virt_bits;

  Vendor _vendor;
  char _model_str[52];

  Unsigned32 _arch_perfmon_info_eax;
  Unsigned32 _arch_perfmon_info_ebx;
  Unsigned32 _arch_perfmon_info_ecx;

  Unsigned32 _monitor_mwait_eax;
  Unsigned32 _monitor_mwait_ebx;
  Unsigned32 _monitor_mwait_ecx;
  Unsigned32 _monitor_mwait_edx;

  Unsigned32 _thermal_and_pm_eax;

  Unsigned32 scaler_tsc_to_ns;
  Unsigned32 scaler_tsc_to_us;
  Unsigned32 scaler_ns_to_tsc;

public:

  void disable(Cpu_number cpu, char const *reason);

  char const *model_str() const { return _model_str; }
  Vendor vendor() const { return _vendor; }

  unsigned family() const
  { return (_version >> 8 & 0xf) + (_version >> 20 & 0xff); }

  char const *vendor_str() const
  { return _vendor == Vendor_unknown ? "Unknown" : vendor_ident[_vendor]; }

  unsigned model() const
  { return (_version >> 4 & 0xf) + (_version >> 12 & 0xf0); }

  unsigned stepping() const { return _version & 0xF; }
  unsigned type() const { return (_version >> 12) & 0x3; }
  Unsigned64 frequency() const { return _frequency; }
  unsigned brand() const { return _brand & 0xFF; }
  unsigned features() const { return _features; }
  unsigned ext_features() const { return _ext_features; }
  bool has_monitor_mwait() const { return _ext_features & (1 << 3); }
  bool has_monitor_mwait_irq() const { return _monitor_mwait_ecx & 3; }

  bool __attribute__((const)) has_smep() const
  { return _ext_07_ebx & FEATX_SMEP; }

  unsigned ext_8000_0001_ecx() const { return _ext_8000_0001_ecx; }
  unsigned ext_8000_0001_edx() const { return _ext_8000_0001_edx; }
  unsigned local_features() const { return _local_features; }
  bool superpages() const { return features() & FEAT_PSE; }
  bool tsc() const { return features() & FEAT_TSC; }
  bool sysenter() const { return features() & FEAT_SEP; }
  bool syscall() const { return ext_8000_0001_edx() & FEATA_SYSCALL; }
  bool vmx() { return boot_cpu()->ext_features() & FEATX_VMX; }
  bool svm() { return boot_cpu()->ext_8000_0001_ecx() & FEATA_SVM; }
  bool has_amd_osvw() { return  boot_cpu()->ext_8000_0001_ecx() & (1<<9); }
  unsigned virt_bits() const { return _virt_bits; }
  unsigned phys_bits() const { return _phys_bits; }
  Unsigned32 get_scaler_tsc_to_ns() const { return scaler_tsc_to_ns; }
  Unsigned32 get_scaler_tsc_to_us() const { return scaler_tsc_to_us; }
  Unsigned32 get_scaler_ns_to_tsc() const { return scaler_ns_to_tsc; }

  Address volatile &kernel_sp() const;

public:
  static Per_cpu<Cpu> cpus;
  static Cpu *boot_cpu() { return _boot_cpu; }

  static bool have_superpages() { return boot_cpu()->superpages(); }
  static bool have_sysenter() { return boot_cpu()->sysenter(); }
  static bool have_syscall() { return boot_cpu()->syscall(); }
  static bool have_fxsr() { return boot_cpu()->features() & FEAT_FXSR; }
  static bool have_pge() { return boot_cpu()->features() & FEAT_PGE; }
  static bool have_xsave() { return boot_cpu()->ext_features() & FEATX_XSAVE; }

  bool has_xsave() const { return ext_features() & FEATX_XSAVE; }

private:

  static Cpu *_boot_cpu;

  struct Vendor_table {
    Unsigned32 vendor_mask;
    Unsigned32 vendor_code;
    Unsigned16 l2_cache;
    char       vendor_string[32];
  } __attribute__((packed));

  struct Cache_table {
    Unsigned8  desc;
    Unsigned8  level;
    Unsigned16 size;
    Unsigned8  asso;
    Unsigned8  line_size;
  };

  static Vendor_table const intel_table[];
  static Vendor_table const amd_table[];
  static Vendor_table const cyrix_table[];
  static Vendor_table const via_table[];
  static Vendor_table const umc_table[];
  static Vendor_table const nexgen_table[];
  static Vendor_table const rise_table[];
  static Vendor_table const transmeta_table[];
  static Vendor_table const sis_table[];
  static Vendor_table const nsc_table[];

  static Cache_table const intel_cache_table[];

  static char const * const vendor_ident[];
  static Vendor_table const * const vendor_table[];

  static char const * const exception_strings[];
};


//-----------------------------------------------------------------------------
/*
 * Fiasco ia32-native
 * Architecture specific cpu init code
 */
INTERFACE [ia32, amd64]:

#include "l4_types.h"
#include "initcalls.h"
#include "per_cpu_data.h"
#include "gdt.h"
#include "lock_guard.h"
#include "spin_lock.h"

class Gdt;
class Tss;


EXTENSION class Cpu
{
public:
  enum Lbr
  {
    Lbr_uninitialized = 0,
    Lbr_unsupported,
    Lbr_pentium_6,
    Lbr_pentium_4,
    Lbr_pentium_4_ext,
  };

  enum Bts
  {
    Bts_uninitialized = 0,
    Bts_unsupported,
    Bts_pentium_m,
    Bts_pentium_4,
  };

private:
  /** Flags if lbr or bts facilities are activated, used by double-fault
   *  handler to reset the debugging facilities
   */
  Unsigned32 debugctl_busy;

  /** debugctl value for activating lbr or bts */
  Unsigned32 debugctl_set;

  /** debugctl value to reset activated lr/bts facilities in the double-faukt
   *  handler
   */
  Unsigned32 debugctl_reset;

  /** supported lbr type */
  Lbr _lbr;

  /** supported bts type */
  Bts _bts;

  /** is lbr active ? */
  char lbr_active;

  /** is btf active ? */
  char btf_active;

  /** is bts active ? */
  char bts_active;

  Gdt *gdt;
  Tss *tss;
  Tss *tss_dbf;

public:
  Lbr lbr_type() const { return _lbr; }
  Bts bts_type() const { return _bts; }
  bool lbr_status() const { return lbr_active; }
  bool bts_status() const { return bts_active; }
  bool btf_status() const { return btf_active; }

  Gdt* get_gdt() const { return gdt; }
  Tss* get_tss() const { return tss; }
  void set_gdt() const
  {
    Pseudo_descriptor desc((Address)gdt, Gdt::gdt_max-1);
    Gdt::set (&desc);
  }

  static void set_tss() { set_tr(Gdt::gdt_tss); }

private:
  void init_lbr_type();
  void init_bts_type();

};

//-----------------------------------------------------------------------------
IMPLEMENTATION[ia32,amd64,ux]:

#include <cstdio>
#include <cstring>
#include "config.h"
#include "panic.h"
#include "processor.h"

DEFINE_PER_CPU_P(0) Per_cpu<Cpu> Cpu::cpus(Per_cpu_data::Cpu_num);
Cpu *Cpu::_boot_cpu;


Cpu::Vendor_table const Cpu::intel_table[] FIASCO_INITDATA_CPU =
{
  { 0xf0fF0, 0x00400, 0xFFFF, "i486 DX-25/33"                   },
  { 0xf0fF0, 0x00410, 0xFFFF, "i486 DX-50"                      },
  { 0xf0fF0, 0x00420, 0xFFFF, "i486 SX"                         },
  { 0xf0fF0, 0x00430, 0xFFFF, "i486 DX/2"                       },
  { 0xf0fF0, 0x00440, 0xFFFF, "i486 SL"                         },
  { 0xf0fF0, 0x00450, 0xFFFF, "i486 SX/2"                       },
  { 0xf0fF0, 0x00470, 0xFFFF, "i486 DX/2-WB"                    },
  { 0xf0fF0, 0x00480, 0xFFFF, "i486 DX/4"                       },
  { 0xf0fF0, 0x00490, 0xFFFF, "i486 DX/4-WB"                    },
  { 0xf0fF0, 0x00500, 0xFFFF, "Pentium A-Step"                  },
  { 0xf0fF0, 0x00510, 0xFFFF, "Pentium P5"                      },
  { 0xf0fF0, 0x00520, 0xFFFF, "Pentium P54C"                    },
  { 0xf0fF0, 0x00530, 0xFFFF, "Pentium P24T Overdrive"          },
  { 0xf0fF0, 0x00540, 0xFFFF, "Pentium P55C MMX"                },
  { 0xf0fF0, 0x00570, 0xFFFF, "Pentium Mobile"                  },
  { 0xf0fF0, 0x00580, 0xFFFF, "Pentium MMX Mobile (Tillamook)"  },
  { 0xf0fF0, 0x00600, 0xFFFF, "Pentium-Pro A-Step"              },
  { 0xf0fF0, 0x00610, 0xFFFF, "Pentium-Pro"                     },
  { 0xf0fF0, 0x00630, 512,    "Pentium II (Klamath)"            },
  { 0xf0fF0, 0x00640, 512,    "Pentium II (Deschutes)"          },
  { 0xf0fF0, 0x00650, 1024,   "Pentium II (Drake)"              },
  { 0xf0fF0, 0x00650, 512,    "Pentium II (Deschutes)"          },
  { 0xf0fF0, 0x00650, 256,    "Pentium II Mobile (Dixon)"       },
  { 0xf0fF0, 0x00650, 0,      "Celeron (Covington)"             },
  { 0xf0fF0, 0x00660, 128,    "Celeron (Mendocino)"             },
  { 0xf0fF0, 0x00670, 1024,   "Pentium III (Tanner)"            },
  { 0xf0fF0, 0x00670, 512,    "Pentium III (Katmai)"            },
  { 0xf0fF0, 0x00680, 256,    "Pentium III (Coppermine)"        },
  { 0xf0fF0, 0x00680, 128,    "Celeron (Coppermine)"            },
  { 0xf0fF0, 0x00690, 1024,   "Pentium-M (Banias)"              },
  { 0xf0fF0, 0x00690, 512,    "Celeron-M (Banias)"              },
  { 0xf0fF0, 0x006a0, 1024,   "Pentium III (Cascades)"          },
  { 0xf0fF0, 0x006b0, 512,    "Pentium III-S"                   },
  { 0xf0fF0, 0x006b0, 256,    "Pentium III (Tualatin)"          },
  { 0xf0fF0, 0x006d0, 2048,   "Pentium-M (Dothan)"              },
  { 0xf0fF0, 0x006d0, 1024,   "Celeron-M (Dothan)"              },
  { 0xf0fF0, 0x006e0, 2048,   "Core (Yonah)"                    },
  { 0xf0fF0, 0x006f0, 2048,   "Core 2 (Merom)"                  },
  { 0xf0f00, 0x00700, 0xFFFF, "Itanium (Merced)"                },
  { 0xf0fF0, 0x00f00, 256,    "Pentium 4 (Willamette/Foster)"   },
  { 0xf0fF0, 0x00f10, 256,    "Pentium 4 (Willamette/Foster)"   },
  { 0xf0fF0, 0x00f10, 128,    "Celeron (Willamette)"            },
  { 0xf0fF0, 0x00f20, 512,    "Pentium 4 (Northwood/Prestonia)" },
  { 0xf0fF0, 0x00f20, 128,    "Celeron (Northwood)"             },
  { 0xf0fF0, 0x00f30, 1024,   "Pentium 4E (Prescott/Nocona)"    },
  { 0xf0fF0, 0x00f30, 256,    "Celeron D (Prescott)"            },
  { 0xf0fF4, 0x00f40, 1024,   "Pentium 4E (Prescott/Nocona)"    },
  { 0xf0fF4, 0x00f44, 1024,   "Pentium D (Smithfield)"          },
  { 0xf0fF0, 0x00f40, 256,    "Celeron D (Prescott)"            },
  { 0xf0fF0, 0x00f60, 2048,   "Pentium D (Cedarmill/Presler)"   },
  { 0xf0fF0, 0x00f60, 512,    "Celeron D (Cedarmill)"           },
  { 0xf0ff0, 0x10600,   0,    "Celeron, 65nm"                   },
  { 0xf0ff0, 0x10670, 2048,   "Core2 / Xeon (Wolfdale), 45nm"   },
  { 0xf0ff0, 0x106a0, 0xffff, "Core i7 / Xeon, 45nm"            },
  { 0xf0ff0, 0x106b0, 0xffff, "Xeon MP, 45nm"                   },
  { 0xf0ff0, 0x106c0, 0xffff, "Atom"                            },
  { 0x0,     0x0,     0xFFFF, ""                                }
};

Cpu::Vendor_table const Cpu::amd_table[] FIASCO_INITDATA_CPU =
{
  { 0xFF0, 0x430, 0xFFFF, "Am486DX2-WT"                     },
  { 0xFF0, 0x470, 0xFFFF, "Am486DX2-WB"                     },
  { 0xFF0, 0x480, 0xFFFF, "Am486DX4-WT / Am5x86-WT"         },
  { 0xFF0, 0x490, 0xFFFF, "Am486DX4-WB / Am5x86-WB"         },
  { 0xFF0, 0x4a0, 0xFFFF, "SC400"                           },
  { 0xFF0, 0x4e0, 0xFFFF, "Am5x86-WT"                       },
  { 0xFF0, 0x4f0, 0xFFFF, "Am5x86-WB"                       },
  { 0xFF0, 0x500, 0xFFFF, "K5 (SSA/5) PR75/90/100"          },
  { 0xFF0, 0x510, 0xFFFF, "K5 (Godot) PR120/133"            },
  { 0xFF0, 0x520, 0xFFFF, "K5 (Godot) PR150/166"            },
  { 0xFF0, 0x530, 0xFFFF, "K5 (Godot) PR200"                },
  { 0xFF0, 0x560, 0xFFFF, "K6 (Little Foot)"                },
  { 0xFF0, 0x570, 0xFFFF, "K6 (Little Foot)"                },
  { 0xFF0, 0x580, 0xFFFF, "K6-II (Chomper)"                 },
  { 0xFF0, 0x590, 256,    "K6-III (Sharptooth)"             },
  { 0xFF0, 0x5c0, 128,    "K6-2+"                           },
  { 0xFF0, 0x5d0, 256,    "K6-3+"                           },
  { 0xFF0, 0x600, 0xFFFF, "Athlon K7 (Argon)"               },
  { 0xFF0, 0x610, 0xFFFF, "Athlon K7 (Pluto)"               },
  { 0xFF0, 0x620, 0xFFFF, "Athlon K75 (Orion)"              },
  { 0xFF0, 0x630, 64,     "Duron (Spitfire)"                },
  { 0xFF0, 0x640, 256,    "Athlon (Thunderbird)"            },
  { 0xFF0, 0x660, 256,    "Athlon (Palomino)"               },
  { 0xFF0, 0x660, 64,     "Duron (Morgan)"                  },
  { 0xFF0, 0x670, 64,     "Duron (Morgan)"                  },
  { 0xFF0, 0x680, 256,    "Athlon (Thoroughbred)"           },
  { 0xFF0, 0x680, 64,     "Duron (Applebred)"               },
  { 0xFF0, 0x6A0, 512,    "Athlon (Barton)"                 },
  { 0xFF0, 0x6A0, 256,    "Athlon (Thorton)"                },
  { 0xfff0ff0,  0x000f00,   0,      "Athlon 64 (Clawhammer)"       },
  { 0xfff0ff0,  0x000f10,   0,      "Opteron (Sledgehammer)"       },
  { 0xfff0ff0,  0x000f40,   0,      "Athlon 64 (Clawhammer)"       },
  { 0xfff0ff0,  0x000f50,   0,      "Opteron (Sledgehammer)"       },
  { 0xfff0ff0,  0x000fc0,   512,    "Athlon64 (Newcastle)"         },
  { 0xfff0ff0,  0x000fc0,   256,    "Sempron (Paris)"              },
  { 0xfff0ff0,  0x010f50,   0,      "Opteron (Sledgehammer)"       },
  { 0xfff0ff0,  0x010fc0,   0,      "Sempron (Oakville)"           },
  { 0xfff0ff0,  0x010ff0,   0,      "Athlon 64 (Winchester)"       },
  { 0xfff0ff0,  0x020f10,   0,      "Opteron (Jackhammer)"         },
  { 0xfff0ff0,  0x020f30,   0,      "Athlon 64 X2 (Toledo)"        },
  { 0xfff0ff0,  0x020f40,   0,      "Turion 64 (Lancaster)"        },
  { 0xfff0ff0,  0x020f50,   0,      "Opteron (Venus)"              },
  { 0xfff0ff0,  0x020f70,   0,      "Athlon 64 (San Diego)"        },
  { 0xfff0ff0,  0x020fb0,   0,      "Athlon 64 X2 (Manchester)"    },
  { 0xfff0ff0,  0x020fc0,   0,      "Sempron (Palermo)"            },
  { 0xfff0ff0,  0x020ff0,   0,      "Athlon 64 (Venice)"           },
  { 0xfff0ff0,  0x040f10,   0,      "Opteron (Santa Rosa)"         },
  { 0xfff0ff0,  0x040f30,   0,      "Athlon 64 X2 (Windsor)"       },
  { 0xfff0ff0,  0x040f80,   0,      "Turion 64 X2 (Taylor)"        },
  { 0xfff0ff0,  0x060fb0,   0,      "Athlon 64 X2 (Brisbane)"      },
  { 0x0,        0x0,        0,      ""                             }
};

Cpu::Vendor_table const Cpu::cyrix_table[] FIASCO_INITDATA_CPU =
{
  { 0xFF0, 0x440, 0xFFFF, "Gx86 (Media GX)"                 },
  { 0xFF0, 0x490, 0xFFFF, "5x86"                            },
  { 0xFF0, 0x520, 0xFFFF, "6x86 (M1)"                       },
  { 0xFF0, 0x540, 0xFFFF, "GXm"                             },
  { 0xFF0, 0x600, 0xFFFF, "6x86MX (M2)"                     },
  { 0x0,   0x0,   0xFFFF, ""                                }
};

Cpu::Vendor_table const Cpu::via_table[] FIASCO_INITDATA_CPU =
{
  { 0xFF0, 0x540, 0xFFFF, "IDT Winchip C6"                  },
  { 0xFF0, 0x580, 0xFFFF, "IDT Winchip 2A/B"                },
  { 0xFF0, 0x590, 0xFFFF, "IDT Winchip 3"                   },
  { 0xFF0, 0x650, 0xFFFF, "Via Jalapeno (Joshua)"           },
  { 0xFF0, 0x660, 0xFFFF, "Via C5A (Samuel)"                },
  { 0xFF8, 0x670, 0xFFFF, "Via C5B (Samuel 2)"              },
  { 0xFF8, 0x678, 0xFFFF, "Via C5C (Ezra)"                  },
  { 0xFF0, 0x680, 0xFFFF, "Via C5N (Ezra-T)"                },
  { 0xFF0, 0x690, 0xFFFF, "Via C5P (Nehemiah)"              },
  { 0xFF0, 0x6a0, 0xFFFF, "Via C5J (Esther)"                },
  { 0x0,   0x0,   0xFFFF, ""                                }
};

Cpu::Vendor_table const Cpu::umc_table[] FIASCO_INITDATA_CPU =
{
  { 0xFF0, 0x410, 0xFFFF, "U5D"                             },
  { 0xFF0, 0x420, 0xFFFF, "U5S"                             },
  { 0x0,   0x0,   0xFFFF, ""                                }
};

Cpu::Vendor_table const Cpu::nexgen_table[] FIASCO_INITDATA_CPU =
{
  { 0xFF0, 0x500, 0xFFFF, "Nx586"                           },
  { 0x0,   0x0,   0xFFFF, ""                                }
};

Cpu::Vendor_table const Cpu::rise_table[] FIASCO_INITDATA_CPU =
{
  { 0xFF0, 0x500, 0xFFFF, "mP6 (iDragon)"                   },
  { 0xFF0, 0x520, 0xFFFF, "mP6 (iDragon)"                   },
  { 0xFF0, 0x580, 0xFFFF, "mP6 (iDragon II)"                },
  { 0xFF0, 0x590, 0xFFFF, "mP6 (iDragon II)"                },
  { 0x0,   0x0,   0xFFFF, ""                                }
};

Cpu::Vendor_table const Cpu::transmeta_table[] FIASCO_INITDATA_CPU =
{
  { 0xFFF, 0x542, 0xFFFF, "TM3x00 (Crusoe)"                 },
  { 0xFFF, 0x543, 0xFFFF, "TM5x00 (Crusoe)"                 },
  { 0xFF0, 0xf20, 0xFFFF, "TM8x00 (Efficeon)"               },
  { 0x0,   0x0,   0xFFFF, ""                                }
};

Cpu::Vendor_table const Cpu::sis_table[] FIASCO_INITDATA_CPU =
{
  { 0xFF0, 0x500, 0xFFFF, "55x"                             },
  { 0x0,   0x0,   0xFFFF, ""                                }
};

Cpu::Vendor_table const Cpu::nsc_table[] FIASCO_INITDATA_CPU =
{
  { 0xFF0, 0x540, 0xFFFF, "Geode GX1"                       },
  { 0xFF0, 0x550, 0xFFFF, "Geode GX2"                       },
  { 0xFF0, 0x680, 0xFFFF, "Geode NX"                        },
  { 0x0,   0x0,   0xFFFF, ""                                }
};

Cpu::Cache_table const Cpu::intel_cache_table[] FIASCO_INITDATA_CPU =
{
  { 0x01, Tlb_inst_4k,        32,   4,    0 },
  { 0x02, Tlb_inst_4M,         2,   4,    0 },
  { 0x03, Tlb_data_4k,        64,   4,    0 },
  { 0x04, Tlb_data_4M,         8,   4,    0 },
  { 0x05, Tlb_data_4M,        32,   4,    0 },
  { 0x06, Cache_l1_inst,       8,   4,   32 },
  { 0x08, Cache_l1_inst,      16,   4,   32 },
  { 0x09, Cache_l1_inst,      32,   4,   64 },
  { 0x0A, Cache_l1_data,       8,   2,   32 },
  { 0x0B, Tlb_inst_4M,         4,   4,    0 },
  { 0x0C, Cache_l1_data,      16,   4,   32 },
  { 0x0D, Cache_l1_data,      16,   4,   64 },
  { 0x0E, Cache_l1_data,      24,   6,   64 },
  { 0x21, Cache_l2,          256,   8,   64 },
  { 0x22, Cache_l3,          512,   4,   64 },  /* sectored */
  { 0x23, Cache_l3,         1024,   8,   64 },  /* sectored */
  { 0x25, Cache_l3,         2048,   8,   64 },  /* sectored */
  { 0x29, Cache_l3,         4096,   8,   64 },  /* sectored */
  { 0x2C, Cache_l1_data,      32,   8,   64 },
  { 0x30, Cache_l1_inst,      32,   8,   64 },
  { 0x39, Cache_l2,          128,   4,   64 },  /* sectored */
  { 0x3B, Cache_l2,          128,   2,   64 },  /* sectored */
  { 0x3C, Cache_l2,          256,   4,   64 },  /* sectored */
  { 0x41, Cache_l2,          128,   4,   32 },
  { 0x42, Cache_l2,          256,   4,   32 },
  { 0x43, Cache_l2,          512,   4,   32 },
  { 0x44, Cache_l2,         1024,   4,   32 },
  { 0x45, Cache_l2,         2048,   4,   32 },
  { 0x46, Cache_l3,         4096,   4,   64 },
  { 0x47, Cache_l3,         8192,   8,   64 },
  { 0x48, Cache_l2,         3072,  12,   64 },
  { 0x49, Cache_l2,         4096,  16,   64 },
  { 0x4A, Cache_l3,         6144,  12,   64 },
  { 0x4B, Cache_l3,         8192,  16,   64 },
  { 0x4C, Cache_l3,        12288,  12,   64 },
  { 0x4D, Cache_l3,        16384,  16,   64 },
  { 0x4E, Cache_l3,         6144,  24,   64 },
  { 0x4F, Tlb_inst_4k,        32,   0,    0 },
  { 0x50, Tlb_inst_4k_4M,     64,   0,    0 },
  { 0x51, Tlb_inst_4k_4M,    128,   0,    0 },
  { 0x52, Tlb_inst_4k_4M,    256,   0,    0 },
  { 0x56, Tlb_data_4M,        16,   4,    0 },
  { 0x57, Tlb_data_4k,        16,   4,    0 },
  { 0x59, Tlb_data_4k,        16,   0,    0 },
  { 0x5A, Tlb_data_2M_4M,     32,   4,    0 },
  { 0x5B, Tlb_data_4k_4M,     64,   0,    0 },
  { 0x5C, Tlb_data_4k_4M,    128,   0,    0 },
  { 0x5D, Tlb_data_4k_4M,    256,   0,    0 },
  { 0x60, Cache_l1_data,      16,   8,   64 },
  { 0x66, Cache_l1_data,       8,   4,   64 },  /* sectored */
  { 0x67, Cache_l1_data,      16,   4,   64 },  /* sectored */
  { 0x68, Cache_l1_data,      32,   4,   64 },  /* sectored */
  { 0x70, Cache_l1_trace,     12,   8,    0 },
  { 0x71, Cache_l1_trace,     16,   8,    0 },
  { 0x72, Cache_l1_trace,     32,   8,    0 },
  { 0x77, Cache_l1_inst,      16,   4,   64 },
  { 0x78, Cache_l2,         1024,   4,   64 },
  { 0x79, Cache_l2,          128,   8,   64 },  /* sectored */
  { 0x7A, Cache_l2,          256,   8,   64 },  /* sectored */
  { 0x7B, Cache_l2,          512,   8,   64 },  /* sectored */
  { 0x7C, Cache_l2,         1024,   8,   64 },  /* sectored */
  { 0x7D, Cache_l2,         2048,   8,   64 },
  { 0x7E, Cache_l2,          256,   8,  128 },
  { 0x7F, Cache_l2,          512,   2,   64 },
  { 0x80, Cache_l2,          512,  16,   64 },
  { 0x82, Cache_l2,          256,   8,   32 },
  { 0x83, Cache_l2,          512,   8,   32 },
  { 0x84, Cache_l2,         1024,   8,   32 },
  { 0x85, Cache_l2,         2048,   8,   32 },
  { 0x86, Cache_l2,          512,   4,   64 },
  { 0x87, Cache_l2,         1024,   8,   64 },
  { 0x8D, Cache_l3,         3072,  12,  128 },
  { 0xB0, Tlb_inst_4k,       128,   4,    0 },
  { 0xB3, Tlb_data_4k,       128,   4,    0 },
  { 0xB4, Tlb_data_4k,       256,   4,    0 },
  { 0xBA, Tlb_data_4k,        64,   4,    0 },
  { 0xC0, Tlb_data_4k_4M,      8,   4,    0 },
  { 0xCA, Tlb_data_4k_4M,    512,   4,    0 },
  { 0xD0, Cache_l3,          512,   4,   64 },
  { 0xD1, Cache_l3,         1024,   4,   64 },
  { 0xD2, Cache_l3,         2048,   4,   64 },
  { 0xD6, Cache_l3,         1024,   8,   64 },
  { 0xD7, Cache_l3,         2048,   8,   64 },
  { 0xD8, Cache_l3,         4096,   8,   64 },
  { 0xDC, Cache_l3,         1536,  12,   64 },
  { 0xDD, Cache_l3,         3072,  12,   64 },
  { 0xDE, Cache_l3,         6144,  12,   64 },
  { 0xE2, Cache_l3,         2048,  16,   64 },
  { 0xE3, Cache_l3,         4096,  16,   64 },
  { 0xE4, Cache_l3,         8192,  16,   64 },
  { 0xEA, Cache_l3,        12288,  24,   64 },
  { 0xEB, Cache_l3,        18432,  24,   64 },
  { 0xEC, Cache_l3,        24576,  24,   64 },
  { 0x0,  Cache_unknown,       0,   0,    0 }
};

char const * const Cpu::vendor_ident[] =
{
   0,
  "GenuineIntel",
  "AuthenticAMD",
  "CyrixInstead",
  "CentaurHauls",
  "UMC UMC UMC ",
  "NexGenDriven",
  "RiseRiseRise",
  "GenuineTMx86",
  "SiS SiS SiS ",
  "Geode by NSC"
};

Cpu::Vendor_table const * const Cpu::vendor_table[] =
{
  0,
  intel_table,
  amd_table,
  cyrix_table,
  via_table,
  umc_table,
  nexgen_table,
  rise_table,
  transmeta_table,
  sis_table,
  nsc_table
};

char const * const Cpu::exception_strings[] =
{
  /*  0 */ "Divide Error",
  /*  1 */ "Debug",
  /*  2 */ "NMI Interrupt",
  /*  3 */ "Breakpoint",
  /*  4 */ "Overflow",
  /*  5 */ "BOUND Range Exceeded",
  /*  6 */ "Invalid Opcode",
  /*  7 */ "Device Not Available",
  /*  8 */ "Double Fault",
  /*  9 */ "CoProcessor Segment Overrrun",
  /* 10 */ "Invalid TSS",
  /* 11 */ "Segment Not Present",
  /* 12 */ "Stack Segment Fault",
  /* 13 */ "General Protection",
  /* 14 */ "Page Fault",
  /* 15 */ "Reserved",
  /* 16 */ "Floating-Point Error",
  /* 17 */ "Alignment Check",
  /* 18 */ "Machine Check",
  /* 19 */ "SIMD Floating-Point Exception",
  /* 20 */ "Reserved",
  /* 21 */ "Reserved",
  /* 22 */ "Reserved",
  /* 23 */ "Reserved",
  /* 24 */ "Reserved",
  /* 25 */ "Reserved",
  /* 26 */ "Reserved",
  /* 27 */ "Reserved",
  /* 28 */ "Reserved",
  /* 29 */ "Reserved",
  /* 30 */ "Reserved",
  /* 31 */ "Reserved"
};

PUBLIC explicit FIASCO_INIT_CPU
Cpu::Cpu(Cpu_number cpu)
{
  set_id(cpu);
  if (cpu == Cpu_number::boot_cpu())
    {
      _boot_cpu = this;
      set_present(1);
      set_online(1);
    }

  init();
}


PUBLIC static
void
Cpu::init_global_features()
{}

PUBLIC static
char const *
Cpu::exception_string(Mword trapno)
{
  if (trapno > 32)
    return "Maskable Interrupt";
  return exception_strings[trapno];
}

PUBLIC static inline FIASCO_INIT_CPU_AND_PM
void
Cpu::cpuid(Unsigned32 mode,
           Unsigned32 *eax, Unsigned32 *ebx,
           Unsigned32 *ecx, Unsigned32 *edx)
{
  asm volatile ("cpuid" : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                        : "a" (mode));
}

PUBLIC static inline FIASCO_INIT_CPU
void
Cpu::cpuid(Unsigned32 mode, Unsigned32 ecx_val,
           Unsigned32 *eax, Unsigned32 *ebx,
           Unsigned32 *ecx, Unsigned32 *edx)
{
  asm volatile ("cpuid" : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                        : "a" (mode), "c" (ecx_val));
}

PUBLIC
void
Cpu::update_features_info()
{
  cpuid(1, &_version, &_brand, &_ext_features, &_features);
}

PRIVATE FIASCO_INIT_CPU
void
Cpu::cache_tlb_intel()
{
  Unsigned8 desc[16];
  unsigned i, count = 0;
  Cache_table const *table;

  do
    {
      cpuid(2, (Unsigned32 *)(desc),
               (Unsigned32 *)(desc + 4),
               (Unsigned32 *)(desc + 8),
               (Unsigned32 *)(desc + 12));

      for (i = 1; i < 16; i++)
	{
	  // Null descriptor or register bit31 set (reserved)
	  if (!desc[i] || (desc[i / 4 * 4 + 3] & (1 << 7)))
	    continue;

	  for (table = intel_cache_table; table->desc; table++)
	    {
	      if (table->desc == desc[i])
		{
		  switch (table->level)
		    {
		    case Cache_l1_data:
		      _l1_data_cache_size      = table->size;
		      _l1_data_cache_asso      = table->asso;
		      _l1_data_cache_line_size = table->line_size;
		      break;
		    case Cache_l1_inst:
		      _l1_inst_cache_size      = table->size;
		      _l1_inst_cache_asso      = table->asso;
		      _l1_inst_cache_line_size = table->line_size;
		      break;
		    case Cache_l1_trace:
		      _l1_trace_cache_size = table->size;
		      _l1_trace_cache_asso = table->asso;
		      break;
		    case Cache_l2:
		      _l2_cache_size      = table->size;
		      _l2_cache_asso      = table->asso;
		      _l2_cache_line_size = table->line_size;
		      break;
		    case Cache_l3:
		      _l3_cache_size      = table->size;
		      _l3_cache_asso      = table->asso;
		      _l3_cache_line_size = table->line_size;
		      break;
		    case Tlb_inst_4k:
		      _inst_tlb_4k_entries += table->size;
		      break;
		    case Tlb_data_4k:
		      _data_tlb_4k_entries += table->size;
		      break;
		    case Tlb_inst_4M:
		      _inst_tlb_4m_entries += table->size;
		      break;
		    case Tlb_data_4M:
		      _data_tlb_4m_entries += table->size;
		      break;
		    case Tlb_inst_4k_4M:
		      _inst_tlb_4k_4m_entries += table->size;
		      break;
		    case Tlb_data_4k_4M:
		      _data_tlb_4k_4m_entries += table->size;
		      break;
		    default:
		      break;
		    }
		  break;
		}
	    }
	}
    }
  while (++count < *desc);
}

PRIVATE FIASCO_INIT_CPU
void
Cpu::cache_tlb_l1()
{
  Unsigned32 eax, ebx, ecx, edx;
  cpuid(0x80000005, &eax, &ebx, &ecx, &edx);

  _l1_data_cache_size      = (ecx >> 24) & 0xFF;
  _l1_data_cache_asso      = (ecx >> 16) & 0xFF;
  _l1_data_cache_line_size =  ecx        & 0xFF;

  _l1_inst_cache_size      = (edx >> 24) & 0xFF;
  _l1_inst_cache_asso      = (edx >> 16) & 0xFF;
  _l1_inst_cache_line_size =  edx        & 0xFF;

  _data_tlb_4k_entries     = (ebx >> 16) & 0xFF;
  _inst_tlb_4k_entries     =  ebx        & 0xFF;
  _data_tlb_4m_entries     = (eax >> 16) & 0xFF;
  _inst_tlb_4m_entries     =  eax        & 0xFF;
}

PRIVATE FIASCO_INIT_CPU
void
Cpu::cache_tlb_l2_l3()
{
  Unsigned32 eax, ebx, ecx, edx;
  cpuid(0x80000006, &eax, &ebx, &ecx, &edx);

  if (vendor() == Vendor_via)
    {
      _l2_cache_size          = (ecx >> 24) & 0xFF;
      _l2_cache_asso          = (ecx >> 16) & 0xFF;
    }
  else
    {
      _l2_data_tlb_4m_entries = (eax >> 16) & 0xFFF;
      _l2_inst_tlb_4m_entries =  eax        & 0xFFF;
      _l2_data_tlb_4k_entries = (ebx >> 16) & 0xFFF;
      _l2_inst_tlb_4k_entries =  ebx        & 0xFFF;
      _l2_cache_size          = (ecx >> 16) & 0xFFFF;
      _l2_cache_asso          = (ecx >> 12) & 0xF;
    }

  _l2_cache_line_size = ecx & 0xFF;

  _l3_cache_size      = (edx >> 18) << 9;
  _l3_cache_asso      = (edx >> 12) & 0xF;
  _l3_cache_line_size = edx & 0xFF;
}

PRIVATE FIASCO_INIT_CPU
void
Cpu::addr_size_info()
{
  Unsigned32 eax, ebx, ecx, edx;
  cpuid(0x80000008, &eax, &ebx, &ecx, &edx);

  _phys_bits = eax & 0xff;
  _virt_bits = (eax & 0xff00) >> 8;
}

PUBLIC static
unsigned
Cpu::amd_cpuid_mnc()
{
  Unsigned32 eax, ebx, ecx, edx;
  cpuid(0x80000008, &eax, &ebx, &ecx, &edx);

  unsigned apicidcoreidsize = (ecx >> 12) & 0xf;
  if (apicidcoreidsize == 0)
    return (ecx & 0xf) + 1; // NC
  return 1 << apicidcoreidsize;
}

PRIVATE FIASCO_INIT_CPU
void
Cpu::set_model_str()
{
  Vendor_table const *table;

  if (_model_str[0])
    return;

  for (table = vendor_table[vendor()]; table && table->vendor_mask; table++)
    if ((_version & table->vendor_mask) == table->vendor_code &&
        (table->l2_cache == 0xFFFF || _l2_cache_size >= table->l2_cache))
      {
	snprintf(_model_str, sizeof (_model_str), "%s",
		 table->vendor_string);
	return;
      }

  snprintf(_model_str, sizeof (_model_str), "Unknown CPU");
}

PUBLIC inline FIASCO_INIT_CPU
void
Cpu::arch_perfmon_info(Unsigned32 *eax, Unsigned32 *ebx, Unsigned32 *ecx) const
{
  *eax = _arch_perfmon_info_eax;
  *ebx = _arch_perfmon_info_ebx;
  *ecx = _arch_perfmon_info_ecx;
}

PUBLIC static
unsigned long
Cpu::get_features()
{
  Unsigned32 eflags = get_flags();
  // Check for Alignment Check Support
  set_flags(eflags ^ EFLAGS_AC);
  if (((get_flags() ^ eflags) & EFLAGS_AC) == 0)
    return 0;

  // Check for CPUID Support
  set_flags(eflags ^ EFLAGS_ID);
  if (!((get_flags() ^ eflags) & EFLAGS_ID))
    return 0;

  Unsigned32 max;
  char vendor_id[12];

  cpuid(0, &max, (Unsigned32 *)(vendor_id),
                 (Unsigned32 *)(vendor_id + 8),
                 (Unsigned32 *)(vendor_id + 4));

  if (!max)
    return 0;

  Unsigned32 dummy, dummy1, dummy2, features;
  cpuid(1, &dummy, &dummy1, &dummy2, &features);

  return features;
}


/** Identify the CPU features.
    Attention: This function may be called more than once. The reason is
    that enabling a Local APIC that was previously disabled by the BIOS
    may change the processor features. Therefore, this function has to
    be called again after the Local APIC was enabled.
 */
PUBLIC FIASCO_INIT_CPU
void
Cpu::identify()
{
  Unsigned32 eflags = get_flags();

  _phys_bits = 32;
  _virt_bits = 32;

  // Reset members in case we get called more than once
  _inst_tlb_4k_entries    =
  _data_tlb_4k_entries    =
  _inst_tlb_4m_entries    =
  _data_tlb_4m_entries    =
  _inst_tlb_4k_4m_entries =
  _data_tlb_4k_4m_entries = 0;

  // Check for Alignment Check Support
  set_flags(eflags ^ EFLAGS_AC);
  // FIXME: must not panic at cpu hotplug
  if (((get_flags() ^ eflags) & EFLAGS_AC) == 0)
    panic("CPU too old");

  // Check for CPUID Support
  set_flags(eflags ^ EFLAGS_ID);
  if ((get_flags() ^ eflags) & EFLAGS_ID) {

    Unsigned32 max, i;
    char vendor_id[12];

    cpuid(0, &max, (Unsigned32 *)(vendor_id),
                   (Unsigned32 *)(vendor_id + 8),
                   (Unsigned32 *)(vendor_id + 4));

    for (i = sizeof (vendor_ident) / sizeof (*vendor_ident) - 1; i; i--)
      if (!memcmp(vendor_id, vendor_ident[i], 12))
        break;

    _vendor = (Cpu::Vendor)i;

    switch (max)
      {
      default:
	// All cases fall through!
      case 10:
        cpuid(10, &_arch_perfmon_info_eax,
                  &_arch_perfmon_info_ebx,
                  &_arch_perfmon_info_ecx, &i);
        // FALLTHRU
      case 2:
        if (_vendor == Vendor_intel)
          cache_tlb_intel();
        // FALLTHRU
      case 1:
        update_features_info();
      }

    if (max >= 5 && has_monitor_mwait())
      cpuid(5, &_monitor_mwait_eax, &_monitor_mwait_ebx,
               &_monitor_mwait_ecx, &_monitor_mwait_edx);

    _thermal_and_pm_eax = 0;
    if (max >= 6 && _vendor == Vendor_intel)
      {
        Unsigned32 dummy;
        cpuid(6, &_thermal_and_pm_eax, &dummy, &dummy, &dummy);
      }

    try_enable_hw_performance_states(false);

    if (max >= 7 && _vendor == Vendor_intel)
      {
        Unsigned32 dummy1, dummy2, dummy3;
        cpuid(0x7, 0, &dummy1, &_ext_07_ebx, &dummy2, &dummy3);
      }

    if (_vendor == Vendor_intel)
      {
	switch (family())
	  {
	  case 5:
	    // Avoid Pentium Erratum 74
	    if ((_features & FEAT_MMX) &&
		(model() != 4 ||
		 (stepping() != 4 && (stepping() != 3 || type() != 1))))
	      _local_features |= Lf_rdpmc;
	    break;
	  case 6:
	    // Avoid Pentium Pro Erratum 26
	    if (model() >= 3 || stepping() > 9)
	      _local_features |= Lf_rdpmc;
	    break;
	  case 15:
	    _local_features |= Lf_rdpmc;
	    _local_features |= Lf_rdpmc32;
	    break;
	  }
      }
    else if (_vendor == Vendor_amd)
      {
	switch (family())
	  {
	  case 6:
	  case 15:
	    _local_features |= Lf_rdpmc;
	    break;
	  }
      }

    // Get maximum number for extended functions
    cpuid(0x80000000, &max, &i, &i, &i);

    if (max > 0x80000000)
      {
	switch (max)
	  {
	  default:
	    // FALLTHRU
	  case 0x80000008:
	    if (_vendor == Vendor_amd || _vendor == Vendor_intel)
	      addr_size_info();
	    // FALLTHRU
	  case 0x80000007:
	  case 0x80000006:
	    if (_vendor == Vendor_amd || _vendor == Vendor_via)
	      cache_tlb_l2_l3();
	    // FALLTHRU
	  case 0x80000005:
	    if (_vendor == Vendor_amd || _vendor == Vendor_via)
	      cache_tlb_l1();
	    // FALLTHRU
	  case 0x80000004:
	    {
	      Unsigned32 *s = (Unsigned32 *)_model_str;
	      for (unsigned i = 0; i < 3; ++i)
	        cpuid(0x80000002 + i, &s[0 + 4*i], &s[1 + 4*i],
                                      &s[2 + 4*i], &s[3 + 4*i]);
	      _model_str[48] = 0;
	    }
	    // FALLTHRU
	  case 0x80000003:
	  case 0x80000002:
	  case 0x80000001:
	    if (_vendor == Vendor_intel || _vendor == Vendor_amd)
              cpuid(0x80000001, &i, &i, &_ext_8000_0001_ecx,
                                &_ext_8000_0001_edx);
	    break;
	  }
      }

    // see Intel Spec on SYSENTER:
    // Some Pentium Pro pretend to have it, but actually lack it
    if ((_version & 0xFFF) < 0x633)
      _features &= ~FEAT_SEP;

  } else
    _version = 0x400;

  set_model_str();

  set_flags(eflags);
}

PUBLIC inline NEEDS["processor.h"]
void
Cpu::busy_wait_ns(Unsigned64 ns)
{
  Unsigned64 stop = rdtsc () + ns_to_tsc(ns);

  while (rdtsc() < stop)
    Proc::pause();
}

PUBLIC
bool
Cpu::if_show_infos() const
{
  return    id() == Cpu_number::boot_cpu()
         || !boot_cpu()
         || family()    != boot_cpu()->family()
         || model()     != boot_cpu()->model()
         || stepping()  != boot_cpu()->stepping()
         || brand()     != boot_cpu()->brand();
}

PUBLIC
void
Cpu::show_cache_tlb_info(const char *indent) const
{
  if (!if_show_infos())
    return;

  char s[16];

  *s = '\0';
  if (_l2_inst_tlb_4k_entries)
    snprintf(s, sizeof(s), "/%d", _l2_inst_tlb_4k_entries);
  if (_inst_tlb_4k_entries)
    printf("%s%4d%s Entry I TLB (4K pages)", indent, _inst_tlb_4k_entries, s);
  *s = '\0';
  if (_l2_inst_tlb_4m_entries)
    snprintf(s, sizeof(s), "/%d", _l2_inst_tlb_4k_entries);
  if (_inst_tlb_4m_entries)
    printf("   %4d%s Entry I TLB (4M pages)", _inst_tlb_4m_entries, s);
  if (_inst_tlb_4k_4m_entries)
    printf("%s%4d Entry I TLB (4K or 4M pages)",
           indent, _inst_tlb_4k_4m_entries);
  if (_inst_tlb_4k_entries || _inst_tlb_4m_entries || _inst_tlb_4k_4m_entries)
    putchar('\n');
  *s = '\0';
  if (_l2_data_tlb_4k_entries)
    snprintf(s, sizeof(s), "/%d", _l2_data_tlb_4k_entries);
  if (_data_tlb_4k_entries)
    printf("%s%4d%s Entry D TLB (4K pages)", indent, _data_tlb_4k_entries, s);
  *s = '\0';
  if (_l2_data_tlb_4m_entries)
    snprintf(s, sizeof(s), "/%d", _l2_data_tlb_4m_entries);
  if (_data_tlb_4m_entries)
    printf("   %4d%s Entry D TLB (4M pages)", _data_tlb_4m_entries, s);
  if (_data_tlb_4k_4m_entries)
    printf("%s%4d Entry D TLB (4k or 4M pages)",
           indent, _data_tlb_4k_4m_entries);
  if (_data_tlb_4k_entries || _data_tlb_4m_entries || _data_tlb_4k_4m_entries)
    putchar('\n');

  if (_l1_trace_cache_size)
    printf("%s%3dK %c-ops T Cache (%d-way associative)\n",
           indent, _l1_trace_cache_size, Config::char_micro,
           _l1_trace_cache_asso);

  else if (_l1_inst_cache_size)
    printf("%s%4d KB L1 I Cache (%d-way associative, %d bytes per line)\n",
           indent, _l1_inst_cache_size, _l1_inst_cache_asso,
           _l1_inst_cache_line_size);

  if (_l1_data_cache_size)
    printf("%s%4d KB L1 D Cache (%d-way associative, %d bytes per line)\n"
           "%s%4d KB L2 U Cache (%d-way associative, %d bytes per line)\n",
           indent, _l1_data_cache_size, _l1_data_cache_asso,
           _l1_data_cache_line_size,
           indent, _l2_cache_size, _l2_cache_asso, _l2_cache_line_size);

  if (_l3_cache_size)
    printf("%s%4u KB L3 U Cache (%d-way associative, %d bytes per line)\n",
           indent, _l3_cache_size, _l3_cache_asso, _l3_cache_line_size);
}

IMPLEMENT
void
Cpu::disable(Cpu_number cpu, char const *reason)
{
  printf("CPU%u: is disabled: %s\n", cxx::int_value<Cpu_number>(cpu), reason);
}

// Function used for calculating apic scaler
PUBLIC static inline
Unsigned32
Cpu::muldiv(Unsigned32 val, Unsigned32 mul, Unsigned32 div)
{
  Unsigned32 dummy;

  asm volatile ("mull %3 ; divl %4\n\t"
               :"=a" (val), "=d" (dummy)
               : "0" (val),  "d" (mul),  "c" (div));
  return val;
}


PUBLIC static inline
Unsigned16
Cpu::get_cs()
{
  Unsigned16 val;
  asm volatile ("mov %%cs, %0" : "=rm" (val));
  return val;
}

PUBLIC static inline
Unsigned16
Cpu::get_ds()
{
  Unsigned16 val;
  asm volatile ("mov %%ds, %0" : "=rm" (val));
  return val;
}

PUBLIC static inline
Unsigned16
Cpu::get_es()
{
  Unsigned16 val;
  asm volatile ("mov %%es, %0" : "=rm" (val));
  return val;
}

PUBLIC static inline
Unsigned16
Cpu::get_ss()
{
  Unsigned16 val;
  asm volatile ("mov %%ss, %0" : "=rm" (val));
  return val;
}

PUBLIC static inline
void
Cpu::set_ds(Unsigned16 val)
{ asm volatile ("mov %0, %%ds" : : "rm" (val)); }

PUBLIC static inline
void
Cpu::set_es(Unsigned16 val)
{ asm volatile ("mov %0, %%es" : : "rm" (val)); }

//----------------------------------------------------------------------------
IMPLEMENTATION[ia32, amd64]:

#include "config.h"
#include "div32.h"
#include "gdt.h"
#include "globals.h"
#include "initcalls.h"
#include "io.h"
#include "pit.h"
#include "processor.h"
#include "regdefs.h"
#include "tss.h"

EXTENSION class Cpu
{
private:
  Unsigned64 _suspend_tsc;
};

PUBLIC FIASCO_INIT_AND_PM
void
Cpu::pm_suspend()
{
  Gdt_entry tss_entry;
  tss_entry = (*gdt)[Gdt::gdt_tss / 8];
  tss_entry.access &= 0xfd;
  (*gdt)[Gdt::gdt_tss / 8] = tss_entry;
  _suspend_tsc = rdtsc();
}

PUBLIC FIASCO_INIT_AND_PM
void
Cpu::pm_resume()
{
  if (id() != Cpu_number::boot_cpu())
    {
      // the boot CPU restores some state in asm already
      set_gdt();
      set_ldt(0);
      set_ds(Gdt::data_segment());
      set_es(Gdt::data_segment());
      set_ss(Gdt::gdt_data_kernel | Gdt::Selector_kernel);
      set_fs(Gdt::gdt_data_user   | Gdt::Selector_user);
      set_gs(Gdt::gdt_data_user   | Gdt::Selector_user);
      set_cs();

      set_tss();
    }
  init_sysenter();
  wrmsr(_suspend_tsc, MSR_TSC);

  try_enable_hw_performance_states(true);
}

PUBLIC static inline
void
Cpu::set_cr0(unsigned long val)
{ asm volatile ("mov %0, %%cr0" : : "r" (val)); }

PUBLIC static inline
void
Cpu::set_pdbr(unsigned long addr)
{ asm volatile ("mov %0, %%cr3" : : "r" (addr)); }

PUBLIC static inline
void
Cpu::set_cr4(unsigned long val)
{ asm volatile ("mov %0, %%cr4" : : "r" (val)); }

PUBLIC static inline
void
Cpu::set_ldt(Unsigned16 val)
{ asm volatile ("lldt %0" : : "rm" (val)); }


PUBLIC static inline
void
Cpu::set_ss(Unsigned16 val)
{ asm volatile ("mov %0, %%ss" : : "r" (val)); }

PUBLIC static inline
void
Cpu::set_tr(Unsigned16 val)
{ asm volatile ("ltr %0" : : "rm" (val)); }

PUBLIC static inline
void
Cpu::xsetbv(Unsigned64 val, Unsigned32 xcr)
{
  asm volatile ("xsetbv" : : "a" ((Mword)val),
                             "d" ((Mword)(val >> 32)),
                             "c" (xcr));
}

PUBLIC static inline
Unsigned64
Cpu::xgetbv(Unsigned32 xcr)
{
  Unsigned32 eax, edx;
  asm volatile("xgetbv"
               : "=a" (eax),
                 "=d" (edx)
               : "c" (xcr));
  return eax | ((Unsigned64)edx << 32);
}

PUBLIC static inline
Mword
Cpu::get_cr0()
{
  Mword val;
  asm volatile ("mov %%cr0, %0" : "=r" (val));
  return val;
}

PUBLIC static inline
Address
Cpu::get_pdbr()
{ Address addr; asm volatile ("mov %%cr3, %0" : "=r" (addr)); return addr; }

PUBLIC static inline
Mword
Cpu::get_cr4()
{ Mword val; asm volatile ("mov %%cr4, %0" : "=r" (val)); return val; }

PUBLIC static inline
Unsigned16
Cpu::get_ldt()
{ Unsigned16 val; asm volatile ("sldt %0" : "=rm" (val)); return val; }

PUBLIC static inline
Unsigned16
Cpu::get_tr()
{ Unsigned16 val; asm volatile ("str %0" : "=rm" (val)); return val; }

IMPLEMENT inline
int
Cpu::can_wrmsr() const
{ return features() & FEAT_MSR; }

PUBLIC static inline
Unsigned64
Cpu::rdmsr(Unsigned32 reg)
{
  Unsigned32 l,h;

  asm volatile ("rdmsr" : "=a" (l), "=d" (h) : "c" (reg));
  return ((Unsigned64)h << 32) + (Unsigned64)l;
}

PUBLIC static inline
Unsigned64
Cpu::rdpmc(Unsigned32 idx, Unsigned32)
{
  Unsigned32 l,h;

  asm volatile ("rdpmc" : "=a" (l), "=d" (h) : "c" (idx));
  return ((Unsigned64)h << 32) + (Unsigned64)l;
}

PUBLIC static inline
void
Cpu::wrmsr(Unsigned32 low, Unsigned32 high, Unsigned32 reg)
{ asm volatile ("wrmsr" : : "a" (low), "d" (high), "c" (reg)); }

PUBLIC static inline
void
Cpu::wrmsr(Unsigned64 msr, Unsigned32 reg)
{ asm volatile ("wrmsr" : : "a" ((Unsigned32)msr), "d" ((Unsigned32)(msr >> 32)), "c" (reg)); }

PUBLIC static inline
void
Cpu::enable_rdpmc()
{ set_cr4(get_cr4() | CR4_PCE); }


IMPLEMENT FIASCO_INIT_CPU
void
Cpu::init_lbr_type()
{
  _lbr = Lbr_unsupported;

  if (can_wrmsr())
    {
      // Intel
      if (vendor() == Vendor_intel)
	{
	  if (family() == 15)
	    _lbr = model() < 3 ? Lbr_pentium_4 : Lbr_pentium_4_ext; // P4
	  else if (family() >= 6)
	    _lbr = Lbr_pentium_6; // PPro, PIII
	}
      else if (vendor() == Vendor_amd)
	{
	  if ((family() == 6) || (family() == 15))
	    _lbr = Lbr_pentium_6; // K7/K8
	}
    }
}


IMPLEMENT FIASCO_INIT_CPU
void
Cpu::init_bts_type()
{
  _bts = Bts_unsupported;

  if (can_wrmsr() && vendor() == Vendor_intel)
    {
      if (family() == 15 && (rdmsr(0x1A0) & (1<<11)) == 0)
	_bts = Bts_pentium_4;
      if (family() == 6  && (model() == 9 || (model() >= 13 &&
	      model() <= 15)))
	_bts = Bts_pentium_m;
      if (!(features() & FEAT_DS))
	_bts = Bts_unsupported;
    }
}


PUBLIC inline
void
Cpu::lbr_enable(bool on)
{
  if (lbr_type() != Lbr_unsupported)
    {
      if (on)
	{
	  lbr_active    = true;
	  debugctl_set |= 1;
	  debugctl_busy = true;
	}
      else
	{
	  lbr_active    = false;
	  debugctl_set &= ~1;
	  debugctl_busy = lbr_active || bts_active;
	  wrmsr(debugctl_reset, MSR_DEBUGCTLA);
	}
    }
}


PUBLIC inline
void
Cpu::btf_enable(bool on)
{
  if (lbr_type() != Lbr_unsupported)
    {
      if (on)
	{
	  btf_active      = true;
	  debugctl_set   |= 2;
	  debugctl_reset |= 2; /* don't disable bit in kernel */
	  wrmsr(2, MSR_DEBUGCTLA);     /* activate _now_ */
	}
      else
	{
	  btf_active    = false;
	  debugctl_set &= ~2;
	  debugctl_busy = lbr_active || bts_active;
	  wrmsr(debugctl_reset, MSR_DEBUGCTLA);
	}
    }
}


PUBLIC
void
Cpu::bts_enable(bool on)
{
  if (bts_type() != Bts_unsupported)
    {
      if (on)
	{
	  switch (bts_type())
	    {
	    case Bts_pentium_4: bts_active = true; debugctl_set |= 0x0c; break;
	    case Bts_pentium_m: bts_active = true; debugctl_set |= 0xc0; break;
	    default:;
	    }
	  debugctl_busy = lbr_active || bts_active;
	}
      else
	{
	  bts_active = false;
	  switch (bts_type())
	    {
	    case Bts_pentium_4: debugctl_set &= ~0x0c; break;
	    case Bts_pentium_m: debugctl_set &= ~0xc0; break;
	    default:;
	    }
	  debugctl_busy = lbr_active || bts_active;
	  wrmsr(debugctl_reset, MSR_DEBUGCTLA);
	}
    }
}

PUBLIC inline
void
Cpu::debugctl_enable()
{
  if (debugctl_busy)
    wrmsr(debugctl_set, MSR_DEBUGCTLA);
}

PUBLIC inline
void
Cpu::debugctl_disable()
{
  if (debugctl_busy)
    wrmsr(debugctl_reset, MSR_DEBUGCTLA);
}

/*
 * AMD OS-Visible Workaround Information
 * print a warning if a CPU is affected by any known erratum
 */
PUBLIC
void
Cpu::print_errata()
{
  if (vendor() == Vendor_amd && has_amd_osvw() && can_wrmsr())
    {
      Unsigned16 osvw_id_length, i;
      bool affected = false;
      osvw_id_length = rdmsr(0xc0010140) & 0xff;

      for (i = 1; ((i - 1) * 64) < osvw_id_length; i++)
        {
          Unsigned64 osvw_msr = rdmsr(0xc0010140 + i);
          if (osvw_msr != 0)
            {
              printf("\033[31mOSVW_MSR%d = 0x%016llx\033[m\n",
                     i, rdmsr(0xc0010140 + i));
              affected = true;
            }
        }
      if (affected)
        printf("\033[31m#Errata known %d, affected by at least one\033[m\n",
               osvw_id_length);
    }
}

/**
 * Enable hardware controlled performance states (HWP) if available.
 *
 * HWP enables the processor to autonomously select performance states. The OS
 * can hint the CPU at the desired optimizations. For example, a system running
 * on battery may hint the CPU to optimize for low power consumption. We just
 * enable HWP and configure it to select the performance target autonomously.
 *
 * See Intel Manual Volume 3 Chapter 14.4 for details.
 */
PRIVATE FIASCO_INIT_CPU
void
Cpu::try_enable_hw_performance_states(bool resume)
{
  enum
  {
    HWP_SUPPORT = 1 << 7,
    HIGHEST_PERFORMANCE_SHIFT = 0,
    LOWEST_PERFORMANCE_SHIFT = 24
  };

  if (!(_thermal_and_pm_eax & HWP_SUPPORT))
    return;

  // enable
  wrmsr(0x1ULL, MSR_HWP_PM_ENABLE);

  // let the hardware decide on everything (autonomous operation mode)
  Unsigned64 hwp_caps = rdmsr(MSR_HWP_CAPABILITIES);
  // Package_Control (bit 42) = 0
  // Activity_Window (bits 41:32) = 0 (auto)
  // Energy_Performance_Preference (bits 31:24) = 0x80 (default)
  // Desired_Performance (bits 23:16) = 0 (default)
  // Maximum_Performance (bits 15:8) = HIGHEST_PERFORMANCE(hwp_cap)
  // Minimum_Performance (bits 7:0) = LOWEST_PERFORMANCE(hwp_cap)
  Unsigned64 request =
    0x80ULL << 24 |
    (((hwp_caps >> HIGHEST_PERFORMANCE_SHIFT) & 0xf) << 8) |
    ((hwp_caps >> LOWEST_PERFORMANCE_SHIFT) & 0xf);
  wrmsr(request, MSR_HWP_REQUEST);

  if (!resume && id() == Cpu_number::boot_cpu())
    printf("HWP: enabled\n");
}

IMPLEMENT FIASCO_INIT_CPU
void
Cpu::init()
{
  identify();

  init_lbr_type();
  init_bts_type();

  calibrate_tsc();

  Unsigned32 cr4 = get_cr4();

  if (features() & FEAT_FXSR)
    cr4 |= CR4_OSFXSR;

  if (features() & FEAT_SSE)
    cr4 |= CR4_OSXMMEXCPT;

  // enable SMEP if available
  if (has_smep())
    cr4 |= CR4_SMEP;

  set_cr4 (cr4);

  if ((features() & FEAT_TSC) && can_wrmsr())
    {
      if (_ext_07_ebx & FEATX_IA32_TSC_ADJUST)
        wrmsr(0, 0, MSR_IA32_TSC_ADJUST);
      else
        // at least reset time stamp counter (better for debugging)
        wrmsr(0, 0, MSR_TSC);
    }

  if ((features() & FEAT_PAT) && can_wrmsr())
    wrmsr(0x00010406, 0x00070406, MSR_PAT);

  print_errata();
}

PUBLIC
void
Cpu::print_infos() const
{
  if (if_show_infos())
    printf("CPU[%u]: %s (%X:%X:%X:%X)[%08x] Model: %s at %lluMHz\n\n",
           cxx::int_value<Cpu_number>(id()),
           vendor_str(), family(), model(), stepping(), brand(),
           _version, model_str(),
           div32(frequency(), 1000000));

  show_cache_tlb_info("");
}

// Return 2^32 / (tsc clocks per usec)
FIASCO_INIT_CPU
void
Cpu::calibrate_tsc()
{
  const unsigned calibrate_time = 50000 /*us*/ + 1;

  // sanity check
  if (! (features() & FEAT_TSC))
    goto bad_ctc;

  // only do once
  if (scaler_tsc_to_ns)
    return;

  Unsigned64 tsc_start, tsc_end;
  Unsigned32 count, tsc_to_ns_div, dummy;

    {
      static Spin_lock<> _l;
      auto guard = lock_guard(_l);

      Pit::setup_channel2_to_20hz();

      tsc_start = rdtsc ();
      count = 0;
      do
	{
	  count++;
	}
      while ((Io::in8 (0x61) & 0x20) == 0);
      tsc_end = rdtsc ();
    }

  // Error: ECTCNEVERSET
  if (count <= 1)
    goto bad_ctc;

  tsc_end -= tsc_start;

  // prevent overflow in division (CPU too fast)
  if (tsc_end & 0xffffffff00000000LL)
    goto bad_ctc;

  // prevent overflow in division (CPU too slow)
  if ((tsc_end & 0xffffffffL) < calibrate_time)
    goto bad_ctc;

  // tsc_to_ns_div = calibrate_time * 2^32 / tsc
  asm ("divl %2"
       :"=a" (tsc_to_ns_div), "=d" (dummy)
       :"r" ((Unsigned32)tsc_end), "a" (0), "d" (calibrate_time));

  // scaler_tsc_to_ns = (tsc_to_ns_div * 1000) / 32
  // not using muldiv(tsc_to_ns_div, 1000, 1 << 5), as div result > (1 << 32)
  // will get trap0 if system frequency is too low
  scaler_tsc_to_ns  = tsc_to_ns_div * 31;
  scaler_tsc_to_ns += tsc_to_ns_div / 4;
  scaler_tsc_to_us  = tsc_to_ns_div;
  scaler_ns_to_tsc  = muldiv(1 << 31, ((Unsigned32)tsc_end),
                             calibrate_time * 1000 >> 1 * 1 << 5);

  if (scaler_tsc_to_ns)
    _frequency = ns_to_tsc(1000000000UL);

  return;

bad_ctc:
  if (Config::Kip_timer_uses_rdtsc)
    panic("Can't calibrate tsc");
}

IMPLEMENT inline
Unsigned64
Cpu::time_us() const
{
  return tsc_to_us (rdtsc());
}


PUBLIC inline
void
Cpu::enable_ldt(Address addr, int size)
{
  if (!size)
    {
      get_gdt()->clear_entry (Gdt::gdt_ldt / 8);
      set_ldt(0);
    }
  else
    {
      get_gdt()->set_entry_byte(Gdt::gdt_ldt / 8, addr, size-1, 2/*=ldt*/, 0);
      set_ldt(Gdt::gdt_ldt);
    }
}


PUBLIC static inline
Unsigned16
Cpu::get_fs()
{ Unsigned16 val; asm volatile ("mov %%fs, %0" : "=rm" (val)); return val; }

PUBLIC static inline
Unsigned16
Cpu::get_gs()
{ Unsigned16 val; asm volatile ("mov %%gs, %0" : "=rm" (val)); return val; }

PUBLIC static inline
void
Cpu::set_fs(Unsigned16 val)
{ asm volatile ("mov %0, %%fs" : : "rm" (val)); }

PUBLIC static inline
void
Cpu::set_gs(Unsigned16 val)
{ asm volatile ("mov %0, %%gs" : : "rm" (val)); }
