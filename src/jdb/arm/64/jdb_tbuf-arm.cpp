INTERFACE [arm && jdb_logging]:

#define BEGIN_LOG_EVENT(name, sc, fmt)				\
  do								\
    {								\
      Mword __do_log__;						\
      asm volatile ("1:  movz   %0, #0		\n\t"		\
		    ".pushsection \".debug.jdb.log_table\" \n\t"	\
		    "3: .8byte 2f		\n\t"		\
		    "   .8byte 1b		\n\t"		\
		    "   .8byte %c[xfmt]		\n\t"		\
		    ".section \".rodata.log.str\" \n\t"		\
		    "2: .asciz "#name"		\n\t"           \
		    "   .asciz "#sc"		\n\t"		\
		    ".popsection		\n\t"		\
		    : "=r"(__do_log__)                          \
                    : [xfmt] "i" (&Tb_entry_formatter_t<fmt>::singleton));  \
      if (EXPECT_FALSE( __do_log__ ))				\
	{

IMPLEMENTATION [arm && 64bit && jdb_logging]:

IMPLEMENT_OVERRIDE
unsigned char
Jdb_tbuf::get_entry_status(Tb_log_table_entry const *e)
{
  return (*reinterpret_cast<Unsigned32 const *>(e->patch) >> 5) & 0xffff;
}

IMPLEMENT_OVERRIDE
void
Jdb_tbuf::set_entry_status(Tb_log_table_entry const *e,
                           unsigned char value)
{
  Unsigned32 *insn = reinterpret_cast<Unsigned32 *>(e->patch);
  *insn = (*insn & ~(0xffffU << 5)) | (((Unsigned32)value) << 5);
  Mem_unit::make_coherent_to_pou(insn);
}

