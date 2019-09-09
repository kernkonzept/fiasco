INTERFACE [arm && jdb_logging]:

// Typically '.8byte %[xfmt]' would require a 'c' modifier Clang does would
// not accept this. For gcc/ARM64 this works without. Clang bug?
#define BEGIN_LOG_EVENT(name, sc, fmt)				\
  do								\
    {								\
      Mword __do_log__;						\
      asm volatile ("1:  movz   %0, #0		\n\t"		\
		    ".pushsection \".debug.jdb.log_table\" \n\t" \
		    "3: .8byte 2f		\n\t"		\
		    "   .8byte 1b		\n\t"		\
		    "   .8byte %[xfmt]		\n\t"		\
		    ".section \".rodata.log.str\" \n\t"		\
		    "2: .asciz "#name"		\n\t"           \
		    "   .asciz "#sc"		\n\t"		\
		    ".popsection		\n\t"		\
		    : "=r"(__do_log__)                          \
                    : [xfmt] "i" (&Tb_entry_formatter_t<fmt>::singleton)); \
      if (EXPECT_FALSE( __do_log__ ))				\
	{
