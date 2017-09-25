INTERFACE:

EXTENSION class Jdb_bp
{
  enum
  {
    Val_enter      = 0x0000ff00,
    Val_leave      = 0x00000000,
    Val_test_sstep = 0x00004000,
    Val_test       = 0x0000000f,
    Val_test_other = 0x0000e00f,
  };
};
