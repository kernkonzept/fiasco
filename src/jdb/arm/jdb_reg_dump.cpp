IMPLEMENTATION:

#include <cstdio>
#include "jdb.h"

/**
 * 'Reg-Dump' module.
 * 
 * This module handles the 'r' command that
 * dumps the register contents at kernel entry.
 */
class J_reg_dump : public Jdb_module
{
public:
  J_reg_dump() FIASCO_INIT;
  Jdb::Action action();
};


IMPLEMENT
J_reg_dump::J_reg_dump()
  : Jdb_module( Jdb::top(), "regs", "dumps all register contents" , 'r' )
{}

IMPLEMENT
Jdb::Action J_reg_dump::action()
{
  Jdb_regs *r = Jdb::registers();
  printf("\nRegister dump\n"
	 "0: %08x %08x %08x %08x %08x %08x %08x %08x\n"
	 "8: %08x %08x %08x %08x %08x %08x %08x %08x\n",
	 r->r0,r->r1,r->r2,r->r3,r->r4,r->r5,r->r6,
	 r->r7,r->r8,r->r9,r->r10,r->r11,r->r12,r->r13,
	 r->r14,r->r15);

  return Jdb::NOTHING;
}



J_reg_dump register_dumper;


