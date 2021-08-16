INTERFACE [riscv && 32bit && jdb_logging]:

#define BEGIN_LOG_EVENT(name, sc, fmt)             \
  do                                               \
    {                                              \
      Mword __do_log__;                            \
      __asm__ __volatile__ (                       \
        ".option push                          \n" \
        ".option norvc                         \n" \
        "1:  addi %0, zero, 0                  \n" \
        ".option pop                           \n" \
        ".pushsection \".debug.jdb.log_table\" \n" \
        "3: .long 2f  # name                   \n" \
        "   .long 1b  # patch                  \n" \
        "   .long %[xfmt]                      \n" \
        ".section \".rodata.log.str\"          \n" \
        "2: .asciz "#name"                     \n" \
        "   .asciz "#sc"                       \n" \
        ".popsection                           \n" \
        : "=r"(__do_log__)                         \
        : [xfmt] "i" (&Tb_entry_formatter_t<fmt>::singleton));  \
      if (EXPECT_FALSE( __do_log__ ))              \
  {

//----------------------------------------------------------------------------
INTERFACE [riscv && 64bit && jdb_logging]:

#define BEGIN_LOG_EVENT(name, sc, fmt)             \
  do                                               \
    {                                              \
      Mword __do_log__;                            \
      __asm__ __volatile__ (                       \
        ".option push                          \n" \
        ".option norvc                         \n" \
        "1:  addi %0, zero, 0                  \n" \
        ".option pop                           \n" \
        ".pushsection \".debug.jdb.log_table\" \n" \
        "3: .quad 2f  # name                   \n" \
        "   .quad 1b  # patch                  \n" \
        "   .quad %[xfmt]                      \n" \
        ".section \".rodata.log.str\"          \n" \
        "2: .asciz "#name"                     \n" \
        "   .asciz "#sc"                       \n" \
        ".popsection                           \n" \
        : "=r"(__do_log__)                         \
        : [xfmt] "i" (&Tb_entry_formatter_t<fmt>::singleton));  \
      if (EXPECT_FALSE( __do_log__ ))              \
  {
