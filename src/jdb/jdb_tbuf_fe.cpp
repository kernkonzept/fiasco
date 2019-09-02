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

#include "globals.h"
#include "jdb.h"
#include "jdb_types.h"

IMPLEMENT_DEFAULT
unsigned char
Jdb_tbuf_fe::get_entry_status(Tb_log_table_entry const *e)
{
  unsigned char value;
  check(Jdb::peek(Jdb_addr<unsigned char>::kmem_addr(e->patch), value));
  return value;
}

IMPLEMENT_DEFAULT
void
Jdb_tbuf_fe::set_entry_status(Tb_log_table_entry const *e, unsigned char value)
{
  check(Jdb::poke(Jdb_addr<unsigned char>::kmem_addr(e->patch), value));
}
