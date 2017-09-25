//
// Framebuffer handling for Fiasco-UX
//

INTERFACE:

#include "multiboot.h"

class Fb
{
public:
  static void init();
  static void enable();

private:
  static void bootstrap();
  static void setup_mbi();
  static void set_color_mapping (Multiboot_vbe_mode *vbi);
};

// ------------------------------------------------------------------------
IMPLEMENTATION:

#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include "boot_info.h"
#include "initcalls.h"
#include "irq_chip.h"
#include "kmem.h"
#include "irq_chip_ux.h"
#include "warn.h"

IMPLEMENT FIASCO_INIT
void
Fb::bootstrap()
{
  char s_fd[3];
  char s_width[5];
  char s_height[5];
  char s_depth[3];
  char s_fbaddr[15];

  snprintf (s_fd,     sizeof (s_fd),     "%u", Boot_info::fd());
  snprintf (s_fbaddr, sizeof (s_fbaddr), "%lu", Boot_info::fb_phys());
  snprintf (s_width,  sizeof (s_width),  "%u", Boot_info::fb_width());
  snprintf (s_height, sizeof (s_height), "%u", Boot_info::fb_height());
  snprintf (s_depth,  sizeof (s_depth),  "%u", Boot_info::fb_depth());

  execl (Boot_info::fb_program(), Boot_info::fb_program(),
         "-f", s_fd, "-s", s_fbaddr,
         "-x", s_width, "-y", s_height,
         "-d", s_depth, NULL);
}

IMPLEMENT FIASCO_INIT
void
Fb::set_color_mapping(Multiboot_vbe_mode *vbi)
{
  switch (Boot_info::fb_depth())
    {
    case 15:
      vbi->red_mask_size   = 5; vbi->red_field_position   = 10;
      vbi->green_mask_size = 5; vbi->green_field_position = 5;
      vbi->blue_mask_size  = 5; vbi->blue_field_position  = 0;
      break;
    case 16:
      vbi->red_mask_size   = 5; vbi->red_field_position   = 11;
      vbi->green_mask_size = 6; vbi->green_field_position = 5;
      vbi->blue_mask_size  = 5; vbi->blue_field_position  = 0;
      break;
    case 24:
      vbi->red_mask_size   = 8; vbi->red_field_position   = 16;
      vbi->green_mask_size = 8; vbi->green_field_position = 8;
      vbi->blue_mask_size  = 8; vbi->blue_field_position  = 0;
      break;
    case 32:
      vbi->red_mask_size   = 8; vbi->red_field_position   = 16;
      vbi->green_mask_size = 8; vbi->green_field_position = 8;
      vbi->blue_mask_size  = 8; vbi->blue_field_position  = 0;
      break;
    default:
      WARN("Unknown frame buffer color depth %d.", Boot_info::fb_depth());
      break;
    }
}


IMPLEMENT FIASCO_INIT
void
Fb::setup_mbi()
{
  if (!Boot_info::fb_size())
    return;

  Multiboot_info *mbi = Boot_info::mbi_virt();

  struct Multiboot_vbe_controller *vbe = reinterpret_cast
	<Multiboot_vbe_controller *>(Boot_info::mbi_vbe());
  struct Multiboot_vbe_mode *vbi = reinterpret_cast
	<Multiboot_vbe_mode *>((char *) vbe + sizeof (*vbe));

  mbi->flags             |= Multiboot_info::Video_info;
  mbi->vbe_control_info   = Boot_info::mbi_phys()
                             + ((char *) vbe - (char *) mbi);
  mbi->vbe_mode_info      = Boot_info::mbi_phys()
                             + ((char *) vbi - (char *) mbi);
  vbe->total_memory       = Boot_info::fb_size() >> 16;	/* 2^16 == 1024 * 64 */
  vbi->phys_base          = Boot_info::fb_virt();
  vbi->y_resolution       = Boot_info::fb_height();
  vbi->x_resolution       = Boot_info::fb_width();
  vbi->bits_per_pixel     = Boot_info::fb_depth();
  vbi->bytes_per_scanline = Boot_info::fb_width()
                            * ((Boot_info::fb_depth() + 7) >> 3);

  set_color_mapping(vbi);
}

IMPLEMENT FIASCO_INIT
void
Fb::init()
{
  // Check if frame buffer is available
  if (!Boot_info::fb_depth())
    return;

  // setup multiboot info
  setup_mbi();

  auto chip = Irq_chip_ux::main;
  auto const irq = Irq_chip_ux::Irq_con;

  // Setup virtual interrupt
  if (!chip->setup_irq_prov(irq, Boot_info::fb_program(), bootstrap))
    {
      puts ("Problems setting up console interrupt!");
      exit (1);
    }

  auto const pid = chip->pid_for_irq_prov(irq);

  Kip::k()->vhw()->set_desc(Vhw_entry::TYPE_FRAMEBUFFER,
                            Boot_info::fb_virt(), Boot_info::fb_size(),
                            irq, pid, 0);
  Kip::k()->vhw()->set_desc(Vhw_entry::TYPE_INPUT,
                            Boot_info::input_start(), Boot_info::input_size(),
                            irq, pid, 0);

  printf ("Starting Framebuffer: %ux%u@%u\n\n",
          Boot_info::fb_width(), Boot_info::fb_height(),
          Boot_info::fb_depth());
}
