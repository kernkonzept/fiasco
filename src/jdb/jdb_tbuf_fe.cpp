INTERFACE:

#include "tb_entry.h"

class Jdb_tbuf_fe
{
public:
  static unsigned char get_entry_status(Tb_log_table_entry const *e);
  static void set_entry_status(Tb_log_table_entry const *e,
                               unsigned char value);
};

IMPLEMENTATION:

#include "jdb.h"
#include "mem_unit.h"

IMPLEMENT_DEFAULT
unsigned char
Jdb_tbuf_fe::get_entry_status(Tb_log_table_entry const *e)
{
  return *(e->patch);
}

IMPLEMENT_DEFAULT
void
Jdb_tbuf_fe::set_entry_status(Tb_log_table_entry const *e, unsigned char value)
{
  *(e->patch) = value;
  Mem_unit::make_coherent_to_pou(e->patch);
}
