IMPLEMENTATION [arm-debug]:

#include <cstdio>
#include <simpleio.h>
#include "jdb_tbuf.h"
#include "jdb_entry_frame.h"
#include "cpu_lock.h"
#include "vkey.h"
#include "static_init.h"
#include "thread.h"
#include "outer_cache.h"
#include "processor.h"

static void outchar(Thread *, Entry_frame *r)
{ putchar(r->r[0] & 0xff); }

static void outstring(Thread *, Entry_frame *r)
{ putstr((char*)r->r[0]); }

static void outnstring(Thread *, Entry_frame *r)
{ putnstr((char*)r->r[0], r->r[1]); }

static void outdec(Thread *, Entry_frame *r)
{ printf("%ld", r->r[0]); }

static void outhex(Thread *, Entry_frame *r)
{ printf("%08lx", r->r[0]); }

static void outhex20(Thread *, Entry_frame *r)
{ printf("%05lx", r->r[0] & 0xfffff); }

static void outhex16(Thread *, Entry_frame *r)
{ printf("%04lx", r->r[0] & 0xffff); }

static void outhex12(Thread *, Entry_frame *r)
{ printf("%03lx", r->r[0] & 0xfff); }

static void outhex8(Thread *, Entry_frame *r)
{ printf("%02lx", r->r[0] & 0xff); }

static void inchar(Thread *, Entry_frame *r)
{
  r->r[0] = Vkey::get();
  Vkey::clear();
}

static void tbuf(Thread *t, Entry_frame *r)
{
  Mem_space *s = t->mem_space();
  Address ip = r->ip();
  Address_type user;
  Unsigned8 *str;
  int len;
  char c;

  Jdb_entry_frame *entry_frame = reinterpret_cast<Jdb_entry_frame*>((char *)r - 12);
  /* Why the -12? The Jdb_entry_frame has three more members in the beginning
   * (see Trap_state_regs) so we're compensating for this with the -12.
   * Alex: Proper fix?
   */
  user = entry_frame->from_user();

  switch (entry_frame->param())
    {
    case 0: // fiasco_tbuf_get_status()
	{
	  Jdb_status_page_frame *regs =
	    reinterpret_cast<Jdb_status_page_frame*>(entry_frame);
	  regs->set(Mem_layout::Tbuf_ustatus_page);
	}
      break;
    case 1: // fiasco_tbuf_log()
	{
	  Jdb_log_frame *regs = reinterpret_cast<Jdb_log_frame*>(entry_frame);
	  Tb_entry_ke *tb = Jdb_tbuf::new_entry<Tb_entry_ke>();
          str = regs->str();
	  tb->set(t, ip-4);
	  for (len=0; (c = s->peek(str++, user)); len++)
            tb->msg.set_buf(len, c);
          tb->msg.term_buf(len);
          regs->set_tb_entry(tb);
          Jdb_tbuf::commit_entry();
	}
      break;
    case 2: // fiasco_tbuf_clear()
      Jdb_tbuf::clear_tbuf();
      break;
    case 3: // fiasco_tbuf_dump()
      return; // => Jdb
    case 4: // fiasco_tbuf_log_3val()
        {
          // interrupts are disabled in handle_slow_trap()
          Jdb_log_3val_frame *regs =
            reinterpret_cast<Jdb_log_3val_frame*>(entry_frame);
          Tb_entry_ke_reg *tb = Jdb_tbuf::new_entry<Tb_entry_ke_reg>();
          str = regs->str();
          tb->set(t, ip-4);
          tb->v[0] = regs->val1();
          tb->v[1] = regs->val2();
          tb->v[2] = regs->val3();
          for (len=0; (c = s->peek(str++, user)); len++)
            tb->msg.set_buf(len, c);
          tb->msg.term_buf(len);
          regs->set_tb_entry(tb);
          Jdb_tbuf::commit_entry();
        }
      break;
    case 5: // fiasco_tbuf_get_status_phys()
        {
          Jdb_status_page_frame *regs =
            reinterpret_cast<Jdb_status_page_frame*>(entry_frame);
          regs->set(s->virt_to_phys(Mem_layout::Tbuf_ustatus_page));
        }
      break;
    case 6: // fiasco_timer_disable
      printf("JDB: no more timer disable\n");
      //Timer::disable();
      break;
    case 7: // fiasco_timer_enable
      printf("JDB: no more timer enable\n");
      //Timer::enable();
      break;
    case 8: // fiasco_tbuf_log_binary()
      // interrupts are disabled in handle_slow_trap()
      Jdb_log_frame *regs = reinterpret_cast<Jdb_log_frame*>(entry_frame);
      Tb_entry_ke_bin *tb = Jdb_tbuf::new_entry<Tb_entry_ke_bin>();
      str = regs->str();
      tb->set(t, ip-4);
      for (len=0; len < Tb_entry_ke_bin::SIZE; len++)
        tb->set_buf(len, s->peek(str++, user));
      regs->set_tb_entry(tb);
      Jdb_tbuf::commit_entry();
      break;
    }
}

static void do_cli(Thread *, Entry_frame *r)
{ r->psr |= Proc::Status_preempt_disabled; }

static void do_sti(Thread *, Entry_frame *r)
{ r->psr &= ~Proc::Status_preempt_disabled; }

static void getcpu(Thread *, Entry_frame *r)
{
  r->r[0] = cxx::int_value<Cpu_number>(current_cpu());
}

static void init_dbg_extensions()
{
  Thread::dbg_extension[0x01] = &outchar;
  Thread::dbg_extension[0x02] = &outstring;
  Thread::dbg_extension[0x03] = &outnstring;
  Thread::dbg_extension[0x04] = &outdec;
  Thread::dbg_extension[0x05] = &outhex;
  Thread::dbg_extension[0x06] = &outhex20;
  Thread::dbg_extension[0x07] = &outhex16;
  Thread::dbg_extension[0x08] = &outhex12;
  Thread::dbg_extension[0x09] = &outhex8;
  Thread::dbg_extension[0x0d] = &inchar;
  Thread::dbg_extension[0x1d] = &tbuf;

  if (0)
    {
      Thread::dbg_extension[0x32] = &do_cli;
      Thread::dbg_extension[0x33] = &do_sti;
      Thread::dbg_extension[0x34] = &getcpu;
    }
}

STATIC_INITIALIZER(init_dbg_extensions);
