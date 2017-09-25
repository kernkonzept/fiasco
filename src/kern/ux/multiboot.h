#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "types.h"

class Multiboot_header
{
public:
  enum
    {
      /* The entire multiboot_header must be contained
       * within the first MULTIBOOT_SEARCH bytes of the kernel image.  */
      Search = 8192,

      /* Magic value identifying the multiboot_header.  */
      Magic = 0x1badb002,

      /* Features flags for 'flags'.
       * If a boot loader sees a flag in MULTIBOOT_MUSTKNOW set
       * and it doesn't understand it, it must fail.  */
      Mustknow = 0x0000ffff,

      /* Align all boot modules on page (4KB) boundaries.  */
      Page_align = 0x00000001,

      /* Must be provided memory information in multiboot_info structure */
      Memory_info = 0x00000002,

      /* Use the load address fields above instead of the ones in the a.out
       * header to figure out what to load where, and what to do afterwards.
       * This should only be needed for a.out kernel images (ELF and other
       * formats can generally provide the needed information).  */
      Aout_kludge = 0x00010000,

      /* The boot loader passes this value in register EAX to signal the kernel
       * that the multiboot method is being used */
      Valid = 0x2badb002,
    };

  /* Must be MULTIBOOT_MAGIC */
  Unsigned32 magic;

  /* Feature flags - see below.  */
  Unsigned32 flags;

  /* Checksum: magic + flags + checksum == 0 */
  Unsigned32 checksum;

  /* These are only valid if MULTIBOOT_AOUT_KLUDGE is set.  */
  Unsigned32 header_addr;
  Unsigned32 load_addr;
  Unsigned32 load_end_addr;
  Unsigned32 bss_end_addr;
  Unsigned32 entry;
} __attribute__((packed));

/* VBE controller information.  */
class Multiboot_vbe_controller
{
public:
  Unsigned8  signature[4];
  Unsigned16 version;
  Unsigned32 oem_string;
  Unsigned32 capabilities;
  Unsigned32 video_mode;
  Unsigned16 total_memory;
  Unsigned16 oem_software_rev;
  Unsigned32 oem_vendor_name;
  Unsigned32 oem_product_name;
  Unsigned32 oem_product_rev;
  Unsigned8  reserved[222];
  Unsigned8  oem_data[256];
} __attribute__((packed));

/* VBE mode information.  */
class Multiboot_vbe_mode
{
public:
  Unsigned16 mode_attributes;
  Unsigned8  win_a_attributes;
  Unsigned8  win_b_attributes;
  Unsigned16 win_granularity;
  Unsigned16 win_size;
  Unsigned16 win_a_segment;
  Unsigned16 win_b_segment;
  Unsigned32 win_func;
  Unsigned16 bytes_per_scanline;

  /* >=1.2 */
  Unsigned16 x_resolution;
  Unsigned16 y_resolution;
  Unsigned8  x_char_size;
  Unsigned8  y_char_size;
  Unsigned8  number_of_planes;
  Unsigned8  bits_per_pixel;
  Unsigned8  number_of_banks;
  Unsigned8  memory_model;
  Unsigned8  bank_size;
  Unsigned8  number_of_image_pages;
  Unsigned8  reserved0;

  /* direct color */
  Unsigned8 red_mask_size;
  Unsigned8 red_field_position;
  Unsigned8 green_mask_size;
  Unsigned8 green_field_position;
  Unsigned8 blue_mask_size;
  Unsigned8 blue_field_position;
  Unsigned8 reserved_mask_size;
  Unsigned8 reserved_field_position;
  Unsigned8 direct_color_mode_info;

  /* >=2.0 */
  Unsigned32 phys_base;
  Unsigned32 reserved1;
  Unsigned16 reversed2;

  /* >=3.0 */
  Unsigned16 linear_bytes_per_scanline;
  Unsigned8 banked_number_of_image_pages;
  Unsigned8 linear_number_of_image_pages;
  Unsigned8 linear_red_mask_size;
  Unsigned8 linear_red_field_position;
  Unsigned8 linear_green_mask_size;
  Unsigned8 linear_green_field_position;
  Unsigned8 linear_blue_mask_size;
  Unsigned8 linear_blue_field_position;
  Unsigned8 linear_reserved_mask_size;
  Unsigned8 linear_reserved_field_position;
  Unsigned32 max_pixel_clock;

  Unsigned8 reserved3[189];
} __attribute__((packed));

class Multiboot_info
{
public:
  enum
    {
      Memory       = (1L<<0),
      Boot_device  = (1L<<1),
      Cmdline      = (1L<<2),
      Mods         = (1L<<3),
      Aout_syms    = (1L<<4),
      Elf_shdr     = (1L<<5),
      Mem_map      = (1L<<6),
      Drive_info   = (1L<<7),
      Cfg_table    = (1L<<8),
      Boot_ld_name = (1L<<9),
      Apm_table    = (1L<<10),
      Video_info   = (1L<<11),
    };

  /* These flags indicate which parts of the multiboot_info are valid;
   * see below for the actual flag bit definitions.  */
  Unsigned32 flags;

  /* Lower/Upper memory installed in the machine.
   * Valid only if MULTIBOOT_MEMORY is set in flags word above.  */
  Unsigned32 mem_lower;
  Unsigned32 mem_upper;

  /* BIOS disk device the kernel was loaded from.
   * Valid only if MULTIBOOT_BOOT_DEVICE is set in flags word above.  */
  Unsigned8 boot_device[4];

  /* Command-line for the OS kernel: a null-terminated ASCII string.
   * Valid only if MULTIBOOT_CMDLINE is set in flags word above.  */
  Unsigned32 cmdline;

  /* List of boot modules loaded with the kernel.
   * Valid only if MULTIBOOT_MODS is set in flags word above.  */
  Unsigned32 mods_count;
  Unsigned32 mods_addr;

  /* Symbol information for a.out or ELF executables. */
  union
    {
      struct __attribute__((packed))
	{
	  /* a.out symbol information valid only if MULTIBOOT_AOUT_SYMS
	     is set in flags word above.  */
	  Unsigned32  tabsize;	
	  Unsigned32  strsize;
	  Unsigned32  addr;
	  Unsigned32  reserved;
	} a;

      struct __attribute__((packed))
	{
	  /* ELF section header information valid only if
	     MULTIBOOT_ELF_SHDR is set in flags word above.  */
	  Unsigned32  num;
	  Unsigned32  size;
	  Unsigned32  addr;
	  Unsigned32  shndx;
	} e;
    } syms;

  /* Memory map buffer.
     Valid only if MULTIBOOT_MEM_MAP is set in flags word above.  */
  Unsigned32 mmap_count;
  Unsigned32 mmap_addr;

  /* Drive Info buffer */
  Unsigned32 drives_length;
  Unsigned32 drives_addr;

  /* ROM configuration table */
  Unsigned32 config_table;

  /* Boot Loader Name */
  Unsigned32 boot_loader_name;

  /* APM table */
  Unsigned32 apm_table;

  /* Video */
  Unsigned32 vbe_control_info;
  Unsigned32 vbe_mode_info;
  Unsigned16 vbe_mode;
  Unsigned16 vbe_interface_seg;
  Unsigned16 vbe_interface_off;
  Unsigned16 vbe_interface_len;
} __attribute__((packed));

/* The mods_addr field above contains the physical address of the first
   of 'mods_count' multiboot_module structures.  */
class Multiboot_module
{
public:
  /* Physical start and end addresses of the module data itself.  */
  Unsigned32 mod_start;
  Unsigned32 mod_end;

  /* Arbitrary ASCII string associated with the module.  */
  Unsigned32 string;

  /* Boot loader must set to 0; OS must ignore.  */
  Unsigned32 reserved;
} __attribute__((packed));


/* The mmap_addr field above contains the physical address of the first
   of the AddrRangeDesc structure.  "size" represents the size of the
   rest of the structure and optional padding.  The offset to the beginning
   of the next structure is therefore "size + 4".  */
struct AddrRangeDesc
{
  Unsigned32 size;
  Unsigned32 BaseAddrLow;
  Unsigned32 BaseAddrHigh;
  Unsigned32 LengthLow;
  Unsigned32 LengthHigh;
  Unsigned32 Type;
  /* unspecified optional padding... */
} __attribute__((packed));

#endif
