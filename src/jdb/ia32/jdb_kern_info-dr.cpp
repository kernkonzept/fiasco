IMPLEMENTATION[ia32,amd64]:

#include <cstdio>
#include "simpleio.h"

#include "jdb_bp.h"
#include "static_init.h"

class Jdb_kern_info_dr : public Jdb_kern_info_module
{
};

static Jdb_kern_info_dr k_d INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_dr::Jdb_kern_info_dr()
  : Jdb_kern_info_module('d', "Debug registers")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_dr::show()
{
  printf("dr0: " L4_PTR_FMT "\n"
         "dr1: " L4_PTR_FMT "\n"
         "dr2: " L4_PTR_FMT "\n"
         "dr3: " L4_PTR_FMT "\n"
         "dr6: " L4_PTR_FMT "\n"
         "dr7: " L4_PTR_FMT "\n",
	 Jdb_bp::get_dr(0),
	 Jdb_bp::get_dr(1),
	 Jdb_bp::get_dr(2),
	 Jdb_bp::get_dr(3),
	 Jdb_bp::get_dr(6),
	 Jdb_bp::get_dr(7));
}
