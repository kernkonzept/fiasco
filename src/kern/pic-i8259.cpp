INTERFACE[i8259 && (ia32 || amd64)]:

#include "initcalls.h"

EXTENSION class Pic
{
  friend class Jdb_kern_info_pic_state;
  friend class Io_m;
private:
  enum
  {
    MASTER_PIC_BASE = 0x20,
    SLAVES_PIC_BASE = 0xa0,
    OFF_ICW	    = 0x00,
    OFF_OCW	    = 0x01,

    MASTER_ICW	    = MASTER_PIC_BASE + OFF_ICW,
    MASTER_OCW	    = MASTER_PIC_BASE + OFF_OCW,
    SLAVES_ICW	    = SLAVES_PIC_BASE + OFF_ICW,
    SLAVES_OCW	    = SLAVES_PIC_BASE + OFF_OCW,
  };
};


IMPLEMENTATION [i8259 && (ia32 || amd64)]:

#include "io.h"

PUBLIC static inline NEEDS["io.h"]
Unsigned16
Pic::disable_all_save()
{
  Unsigned16 s;
  s  = Io::in8(MASTER_OCW);
  s |= Io::in8(SLAVES_OCW) << 8;
  Io::out8(0xff, MASTER_OCW);
  Io::out8(0xff, SLAVES_OCW);

  return s;
}

PUBLIC static inline NEEDS["io.h"]
void
Pic::restore_all(Unsigned16 s)
{
  Io::out8(s & 0x0ff, MASTER_OCW);
  Io::out8((s >> 8) & 0x0ff, SLAVES_OCW);
}


