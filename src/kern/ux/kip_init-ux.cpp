IMPLEMENTATION [ux]:

#include "boot_info.h"
#include "multiboot.h"
#include "kip.h"
#include <cstring>

PUBLIC static FIASCO_INIT
void
Kip_init::setup_user_virtual(Kip *kinfo)
{
  // start at 64k because on some distributions (like Ubuntu 8.04) it's
  // not allowed to map below a certain treshold
  kinfo->add_mem_region(Mem_desc(Boot_info::min_mappable_address(),
                                 Mem_layout::User_max,
                                 Mem_desc::Conventional, true));
}

PRIVATE static FIASCO_INIT
void
Kip_init::setup_ux(Kip *k)
{
  Multiboot_module *mbm = reinterpret_cast <Multiboot_module*>
    (Kmem::phys_to_virt (Boot_info::mbi_virt()->mods_addr));
  k->user_ptr  = Boot_info::mbi_phys();
  Mem_desc *m  = k->mem_descs_a().begin();

  // start at 64k because on some distributions (like Ubuntu 8.04) it's
  // not allowed to map below a certain treshold
  *(m++) = Mem_desc(Boot_info::min_mappable_address(),
                    ((Boot_info::mbi_virt()->mem_upper + 1024) << 10) - 1,
                    Mem_desc::Conventional);
  *(m++) = Mem_desc(Kmem::kernel_image_start(), Kmem::kcode_end() - 1,
                    Mem_desc::Reserved);

  mbm++;
  k->sigma0_ip		= mbm->reserved;
  if ((Boot_info::sigma0_start() & Config::PAGE_MASK)
      != ((Boot_info::sigma0_end() + (Config::PAGE_SIZE-1))
	   & Config::PAGE_MASK))
    *(m++) = Mem_desc(Boot_info::sigma0_start() & Config::PAGE_MASK,
                      ((Boot_info::sigma0_end() + (Config::PAGE_SIZE - 1))
                       & Config::PAGE_MASK) - 1,
                      Mem_desc::Reserved);

  mbm++;
  k->root_ip		= mbm->reserved;
  if ((Boot_info::root_start() & Config::PAGE_MASK)
      != ((Boot_info::root_end() + (Config::PAGE_SIZE-1)) & Config::PAGE_MASK))
    *(m++) = Mem_desc(Boot_info::root_start() & Config::PAGE_MASK,
                      ((Boot_info::root_end() + (Config::PAGE_SIZE - 1))
                       & Config::PAGE_MASK) - 1,
                      Mem_desc::Bootloader);

  unsigned long version_size = 0;
  for (char const *v = k->version_string(); *v; )
    {
      unsigned l = strlen(v) + 1;
      v += l;
      version_size += l;
    }

  version_size += 2;

  k->vhw_offset = (k->offset_version_strings << 4) + version_size;

  k->vhw()->init();

  unsigned long mod_start = ~0UL;
  unsigned long mod_end = 0;

  mbm++;

  if (Boot_info::mbi_virt()->mods_count <= 3)
    return;

  for (unsigned i = 0; i < Boot_info::mbi_virt()->mods_count - 3; ++i)
    {
      if (mbm[i].mod_start < mod_start)
	mod_start = mbm[i].mod_start;

      if (mbm[i].mod_end > mod_end)
	mod_end = mbm[i].mod_end;
    }

  mod_start &= ~(Config::PAGE_SIZE - 1);
  mod_end = (mod_end + Config::PAGE_SIZE -1) & ~(Config::PAGE_SIZE - 1);

  if (mod_end > mod_start)
    *(m++) = Mem_desc(mod_start, mod_end - 1, Mem_desc::Bootloader);

  *(m++) = Mem_desc(Boot_info::mbi_phys(),
                    ((Boot_info::mbi_phys() + Boot_info::mbi_size()
                     + Config::PAGE_SIZE-1) & Config::PAGE_MASK) - 1,
                    Mem_desc::Bootloader);
}
