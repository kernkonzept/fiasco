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
Jdb_kern_info_apic::show() override
{
  if (!Config::apic)
    {
      puts("Local APIC disabled/not available");
      return;
    }

  for (Cpu_number u = Cpu_number::first(); u < Config::max_num_cpus(); ++u)
    if (Cpu::online(u))
      {
        printf("CPU%u: ", cxx::int_value<Cpu_number>(u));
        auto show_info = [](Cpu_number)
          {
            Apic::id_show(0);
            Apic::timer_show(4);
            Apic::regs_show(4);
            Apic::irr_show(4);
            Apic::isr_show(4);
            putchar('\n');
          };

        if (u == Cpu_number::boot_cpu())
          show_info(u);
        else
          Jdb::remote_work(u, show_info, true);
      }
}
