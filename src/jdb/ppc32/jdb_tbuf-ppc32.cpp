INTERFACE [ppc32 && jdb_logging]:

//#warning TODO: Dummy implementation for PPC32
#define BEGIN_LOG_EVENT(name, sc, fmt)				\
  do								\
    {								\
      Unsigned8 __do_log__ = 0;					\
      if (EXPECT_FALSE( __do_log__ ))				\
	{

