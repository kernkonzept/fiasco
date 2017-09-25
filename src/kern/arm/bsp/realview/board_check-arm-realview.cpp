INTERFACE [arm && realview]:

#include "types.h"

class Board_check
{
public:
  static void check_board();

private:
  static Mword read_board_id();
  struct id_pair
  {
    unsigned mask, id;
  };
  static id_pair ids[];
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && realview]:

#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && realview && realview_eb]:

Board_check::id_pair Board_check::ids[] FIASCO_INITDATA = {
  { 0x1ffffe00, 0x01400400 },
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && realview && realview_pb11mp]:

Board_check::id_pair Board_check::ids[] FIASCO_INITDATA = {
  { 0x0fffff00, 0x0159f500 },
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && realview && realview_pbx]:

Board_check::id_pair Board_check::ids[] FIASCO_INITDATA = {
  { 0xffffff00, 0x1182f500 }, // board
  { 0xffffff00, 0x01780500 }, // qemu
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && realview && realview_vexpress]:

Board_check::id_pair Board_check::ids[] FIASCO_INITDATA = {
  { 0xcfffff00, 0x0190f500 },
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && realview]:

#include "kmem.h"
#include "io.h"
#include "static_init.h"
#include "processor.h"
#include "platform.h"

#include <cstdio>

IMPLEMENT static FIASCO_INIT
Mword
Board_check::read_board_id()
{ return Platform::sys->read<Mword>(Platform::Sys::Id); }

IMPLEMENT static FIASCO_INIT
void
Board_check::check_board()
{
  Mword id = read_board_id();

  printf("Realview System ID: Rev=%lx HBI=%03lx Build=%lx Arch=%lx FPGA=%02lx\n",
         id >> 28, (id >> 16) & 0xfff, (id >> 12) & 0xf,
	 (id >> 8) & 0xf, id & 0xff);

  for (unsigned i = 0; i < (sizeof(ids) / sizeof(ids[0])); ++i)
    if ((id & ids[i].mask) == ids[i].id)
      return;

  printf("  Invalid System ID for this kernel config\n");
  for (unsigned i = 0; i < (sizeof(ids) / sizeof(ids[0])); ++i)
    printf("  Expected (%08lx & %08x) == %08x%s\n",
           id, ids[i].mask, ids[i].id,
           i + 1 < (sizeof(ids) / sizeof(ids[0])) ? ", or" : "");
  printf("  Stopping.\n");
  while (1)
    Proc::halt();
}

STATIC_INITIALIZEX_P(Board_check, check_board, GDB_INIT_PRIO);
