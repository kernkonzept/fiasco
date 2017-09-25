INTERFACE [ux && jdb_logging]:

// We don't want to patch the text segment of Fiasco-UX.

#define BEGIN_LOG_EVENT(name, sc, fmt)				\
  do								\
    {								\
      Unsigned8 __do_log__;					\
      asm volatile (".pushsection \".data\"	\n\t"		\
		    "1:  .byte   0		\n\t"		\
		    ".section \".debug.jdb.log_table\" \n\t"	\
		    ".long 2f			\n\t"		\
		    ".long 1b			\n\t"		\
		    ".long %c[xfmt]		\n\t"		\
		    ".section \".rodata.log.str\" \n\t"		\
		    "2: .asciz "#name"		\n\t"		\
		    "   .asciz "#sc"		\n\t"		\
		    ".popsection		\n\t"		\
		    "movb    1b,%0		\n\t"		\
		    : "=q"(__do_log__) 			\
                    : [xfmt] "i" (&Tb_entry_formatter_t<fmt>::singleton));  \
      if (EXPECT_FALSE( __do_log__ ))				\
	{

