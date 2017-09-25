IMPLEMENTATION:

#include "jdb_kern_info.h"
#include "region.h"
#include "static_init.h"

class Jdb_kern_info_region : public Jdb_kern_info_module
{
};

static Jdb_kern_info_region k_r INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_region::Jdb_kern_info_region()
  : Jdb_kern_info_module('r', "region::debug_dump")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_region::show()
{
  Region::debug_dump();
}

