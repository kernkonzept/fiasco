INTERFACE [mips32 && jdb_logging]:

// Setup Tb_log_table_entry name, patch, fmt.  Must be done such that patching
// the byte at instruction 'patch' can set the value of __do_log__.
#define BEGIN_LOG_EVENT(name, sc, fmt)				\
  do								\
    {								\
      Mword __do_log__;						\
      asm volatile ("1:  addiu %0, $0, 0        \n\t"           \
		    ".pushsection \".debug.jdb.log_table\" \n\t"\
		    "3: .long 2f	# name	\n\t"		\
		    "   .long 1b  	# patch	\n\t"		\
		    "   .long %[xfmt]		\n\t"		\
		    ".section \".rodata.log.str\" \n\t"		\
		    "2: .asciz "#name"		\n\t"           \
		    "   .asciz "#sc"		\n\t"		\
		    ".popsection		\n\t"		\
		    : "=r"(__do_log__)                          \
                    : [xfmt] "i" (&Tb_entry_formatter_t<fmt>::singleton));  \
      if (EXPECT_FALSE( __do_log__ ))				\
	{

INTERFACE [mips64 && jdb_logging]:

// Setup Tb_log_table_entry name, patch, fmt.  Must be done such that patching
// the byte at instruction 'patch' can set the value of __do_log__.
#define BEGIN_LOG_EVENT(name, sc, fmt)				\
  do								\
    {								\
      Mword __do_log__;						\
      asm volatile ("1:  addiu %0, $0, 0        \n\t"           \
		    ".pushsection \".debug.jdb.log_table\" \n\t"\
		    "3: .quad 2f	# name	\n\t"		\
		    "   .quad 1b  	# patch	\n\t"		\
		    "   .quad %[xfmt]		\n\t"		\
		    ".section \".rodata.log.str\" \n\t"		\
		    "2: .asciz "#name"		\n\t"           \
		    "   .asciz "#sc"		\n\t"		\
		    ".popsection		\n\t"		\
		    : "=r"(__do_log__)                          \
                    : [xfmt] "i" (&Tb_entry_formatter_t<fmt>::singleton));  \
      if (EXPECT_FALSE( __do_log__ ))				\
	{
