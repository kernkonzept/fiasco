IMPLEMENTATION [arm && 64bit && jdb_logging]:

#include "globals.h"
#include "jdb.h"
#include "jdb_types.h"

IMPLEMENT_OVERRIDE
unsigned char
Jdb_tbuf_fe::get_entry_status(Tb_log_table_entry const *e)
{
  Unsigned32 insn;
  check(!Jdb::peek_task(Jdb_address::kmem_addr(e->patch), &insn, sizeof(insn)));
  return (insn >> 5) & 0xffff;
}

IMPLEMENT_OVERRIDE
void
Jdb_tbuf_fe::set_entry_status(Tb_log_table_entry const *e, unsigned char value)
{
  Unsigned32 insn;
  check(!Jdb::peek_task(Jdb_address::kmem_addr(e->patch), &insn, sizeof(insn)));
  insn = (insn & ~(0xffffU << 5)) | (((Unsigned32)value) << 5);
  check(!Jdb::poke_task(Jdb_address::kmem_addr(e->patch), &insn, sizeof(insn)));
}

IMPLEMENTATION [arm && thumb2 && jdb_logging]:

#include "globals.h"
#include "jdb.h"
#include "jdb_types.h"

IMPLEMENT_OVERRIDE
unsigned char
Jdb_tbuf_fe::get_entry_status(Tb_log_table_entry const *e)
{
  unsigned char val;
  check(!Jdb::peek_task(Jdb_address::kmem_addr(e->patch + 2), &val, sizeof(val)));
  return val;
}

IMPLEMENT_OVERRIDE
void
Jdb_tbuf_fe::set_entry_status(Tb_log_table_entry const *e, unsigned char value)
{
  check(Jdb::poke(Jdb_addr<unsigned char>::kmem_addr(e->patch + 2), value));
}

