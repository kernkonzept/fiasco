IMPLEMENTATION:

#include <cstdio>
#include "jdb_kern_info.h"
#include "static_init.h"

class Jdb_kern_info_config : public Jdb_kern_info_module
{
};

static Jdb_kern_info_config k_C INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_config::Jdb_kern_info_config()
  : Jdb_kern_info_module('C', "Kernel config")
{
  Jdb_kern_info::register_subcmd(this);
}

extern "C" const char _binary_gblcfg_o_txt_start;
extern "C" const char _binary_gblcfg_o_txt_end;

PUBLIC
void
Jdb_kern_info_config::show() override
{
  printf("%*.*s",
      static_cast<int>(&_binary_gblcfg_o_txt_end - &_binary_gblcfg_o_txt_start),
      static_cast<int>(&_binary_gblcfg_o_txt_end - &_binary_gblcfg_o_txt_start),
      &_binary_gblcfg_o_txt_start);
}
