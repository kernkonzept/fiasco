IMPLEMENTATION [riscv && jdb_logging]:

IMPLEMENT_OVERRIDE
unsigned char
Jdb_tbuf_fe::get_entry_status(Tb_log_table_entry const *e)
{
  Unsigned32 insn;
  check(!Jdb::peek_task(Jdb_address::kmem_addr(e->patch), &insn, sizeof(insn)));
  return (insn >> 20) & 0xfff;
}

IMPLEMENT_OVERRIDE
void
Jdb_tbuf_fe::set_entry_status(Tb_log_table_entry const *e, unsigned char value)
{
  Unsigned32 insn;
  check(!Jdb::peek_task(Jdb_address::kmem_addr(e->patch), &insn, sizeof(insn)));
  insn = (insn & ~(0xfffU << 20)) | (Unsigned32{value} << 20);
  check(!Jdb::poke_task(Jdb_address::kmem_addr(e->patch), &insn, sizeof(insn)));
}
