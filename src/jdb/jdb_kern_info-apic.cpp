IMPLEMENTATION[apic]:

#include <cstdio>
#include "simpleio.h"

#include "apic.h"
#include "static_init.h"

class Jdb_kern_info_apic : public Jdb_kern_info_module
{
};

static Jdb_kern_info_apic k_a INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_apic::Jdb_kern_info_apic()
  : Jdb_kern_info_module('a', "Local APIC state")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_apic::show()
{
  if (!Config::apic)
    {
      puts("Local APIC disabled/not available");
      return;
    }

  Apic::id_show();
  Apic::timer_show();
  Apic::regs_show();
  putchar('\n');
  Apic::irr_show();
  Apic::isr_show();
}

