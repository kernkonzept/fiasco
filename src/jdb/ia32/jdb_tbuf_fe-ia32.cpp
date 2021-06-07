IMPLEMENTATION [(ia32 || amd64) && jdb_logging]:

#include "boot_info.h"
#include "globals.h"
#include "jdb.h"
#include "jdb_types.h"

IMPLEMENT_OVERRIDE
void
Jdb_tbuf_fe::set_entry_status(Tb_log_table_entry const *e, unsigned char value)
{
  check(Jdb::poke(Jdb_addr<unsigned char>::kmem_addr(e->patch), value));
  Boot_info::reset_checksum_ro();
}
