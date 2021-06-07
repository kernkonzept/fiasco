#pragma once

#include "types.h"

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

/* See in L4Re's l4mod.h for details */
class L4mod_info
{
public:
  Unsigned64 flags;
  Unsigned64 cmdline;
  Unsigned64 mods_addr;
  Unsigned32 mods_count;
  Unsigned32 _pad;

  Unsigned64 vbe_control_info;
  Unsigned64 vbe_mode_info;
} __attribute__((packed));

class L4mod_mod
{
public:
  Unsigned64 flags;
  Unsigned64 mod_start;
  Unsigned64 mod_end;
  Unsigned64 cmdline;
} __attribute__((packed));
