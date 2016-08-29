INTERFACE [arm && jdb_logging]:

#define BEGIN_LOG_EVENT(name, sc, fmt)				\
  do								\
    {								\
      Mword __do_log__;						\
      asm volatile ("1:  mov    %0, #0		\n\t"		\
		    ".pushsection \".debug.jdb.log_table\" \n\t"	\
		    "3: .long 2f		\n\t"		\
		    "   .long 1b		\n\t"		\
		    "   .long %c[xfmt]		\n\t"		\
		    ".section \".rodata.log.str\" \n\t"		\
		    "2: .asciz "#name"		\n\t"           \
		    "   .asciz "#sc"		\n\t"		\
		    ".popsection		\n\t"		\
		    : "=r"(__do_log__)                          \
                    : [xfmt] "i" (&Tb_entry_formatter_t<fmt>::singleton));  \
      if (EXPECT_FALSE( __do_log__ ))				\
	{

