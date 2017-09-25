IMPLEMENTATION:

#include "jdb_kern_info.h"
#include "static_init.h"

class Jdb_kern_info_data : public Jdb_kern_info_module
{
};

static Jdb_kern_info_data k_a INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_data::Jdb_kern_info_data()
  : Jdb_kern_info_module('s', "Kernel Data Info")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_data::show()
{
  show_percpu_offsets();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

PRIVATE
void
Jdb_kern_info_data::show_percpu_offsets()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include <cstdio>
#include "config.h"
#include "types.h"
#include "per_cpu_data.h"

PRIVATE
void
Jdb_kern_info_data::show_percpu_offsets()
{
  printf("\n"
         "Percpu offsets:\n");
  for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    printf("  %2u: " L4_PTR_FMT "\n", cxx::int_value<Cpu_number>(i),
                                      (Mword)Per_cpu_data::offset(i));
}
