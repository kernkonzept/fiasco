IMPLEMENTATION[ia32,amd64,ux]:

#include "jdb_dbinfo.h"
#include "jdb_lines.h"
#include "jdb_symbol.h"
#include "jdb_tbuf.h"
#include "thread.h"
#include "timer.h"

/**
 * Handle int3 extensions in the current thread's context. All functions
 * for which we don't need/want to switch to the debugging stack.
 * \return 0 if this function should be handled in the context of Jdb
 *         1 successfully handled
 */
PRIVATE static
int
Jdb::handle_int3_threadctx_generic(Trap_state *ts)
{
  Thread *t  = current_thread();
  Mem_space *s = t->mem_space();

  //Obj_space *os = t->space()->obj_space();
  Address ip = ts->ip();
  Address_type user;
  Unsigned8 *str, todo;
  int len;
  char c;

  Jdb_entry_frame *jef = reinterpret_cast<Jdb_entry_frame*>(ts);
  user = jef->from_user();
  todo = s->peek((Unsigned8*)ip, user);

  switch (todo)
    {
    case 0xeb: // jmp == enter_kdebug()
      len = s->peek((Unsigned8*)(ip+1), user);
      str = (Unsigned8*)(ip + 2);

      if ((len > 0) && s->peek(str, user) == '*')
	{
          int i;

	  // skip '*'
	  len--; str++;

     	  if ((len > 0) && s->peek(str, user) == '#')
	    // special: enter_kdebug("*#...")
 	    return 0; // => Jdb

      	  if (s->peek(str, user) == '+')
	    {
	      // special: enter_kdebug("*+...") => extended log msg
	      // skip '+'
	      len--; str++;
	      Tb_entry_ke_reg *tb = Jdb_tbuf::new_entry<Tb_entry_ke_reg>();
	      tb->set(t, ip-1);
              tb->v[0] = ts->value();
              tb->v[1] = ts->value2();
              tb->v[2] = ts->value3();
	      for (i=0; i<len; i++)
      		tb->msg.set_buf(i, s->peek(str++, user));
	      tb->msg.term_buf(len);
	    }
	  else
	    {
	      // special: enter_kdebug("*...") => log entry
	      // fill in entry
	      Tb_entry_ke *tb = Jdb_tbuf::new_entry<Tb_entry_ke>();
	      tb->set(t, ip-1);
	      for (i=0; i<len; i++)
		tb->msg.set_buf(i, s->peek(str++, user));
	      tb->msg.term_buf(len);
	    }
	  Jdb_tbuf::commit_entry();
	  break;
	}
      return 0; // => Jdb

    case 0x90: // nop == l4kd_display()
      if (          s->peek((Unsigned8*)(ip+1), user) != 0xeb /*jmp*/
	  || (len = s->peek((Unsigned8*)(ip+2), user)) <= 0)
	return 0; // => Jdb
      str = (Unsigned8*)(ip + 3);
      for (; len; len--)
	putchar(s->peek(str++, user));
      break;

    case 0x3c: // cmpb
	{
      todo = s->peek((Unsigned8*)(ip+1), user);
      Jdb_output_frame *regs = reinterpret_cast<Jdb_output_frame*>(ts);
      switch (todo)
	{
	case  0: // l4kd_outchar
	  putchar(regs->value() & 0xff);
	  break;
        case  1: // l4kd_outnstring
	  str = regs->str();
    	  len = regs->len();
	  for(; len > 0; len--)
	    putchar(s->peek(str++, user));
	  break;
	case  2: // l4kd_outstr
	  str = regs->str();
	  for (; (c=s->peek(str++, user)); )
      	    putchar(c);
	  break;
	case  5: // l4kd_outhex32 
	  printf("%08lx", regs->value() & 0xffffffff);
	  break;
	case  6: // l4kd_outhex20 
	  printf("%05lx", regs->value() & 0xfffff);
	  break;
	case  7: // l4kd_outhex16 
	  printf("%04lx", regs->value() & 0xffff);
	  break;
	case  8: // L4kd_outhex12
	  printf("%03lx", regs->value() & 0xfff);
	  break;
	case  9: // l4kd_outhex8 
	  printf("%02lx", regs->value() & 0xff);
	  break;
	case 11: // l4kd_outdec
	  printf("%lu", regs->value());
	  break;
	case 13: // l4kd_inchar
	  return 0; // => Jdb
	case 29:
	  switch (jef->param())
	    {
	    case 0: // fiasco_tbuf_get_status()
		{
		  Jdb_status_page_frame *regs =
		    reinterpret_cast<Jdb_status_page_frame*>(ts);
		  regs->set(Mem_layout::Tbuf_ustatus_page);
		}
	      break;
	    case 1: // fiasco_tbuf_log()
		{
		  // interrupts are disabled in handle_slow_trap()
		  Jdb_log_frame *regs = reinterpret_cast<Jdb_log_frame*>(ts);
		  Tb_entry_ke *tb =
		    static_cast<Tb_entry_ke*>(Jdb_tbuf::new_entry());
		  str = regs->str();
		  tb->set(t, ip-1);
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
	      return 0; // => Jdb
	    case 4: // fiasco_tbuf_log_3val()
		{
		  // interrupts are disabled in handle_slow_trap()
		  Jdb_log_3val_frame *regs = 
		    reinterpret_cast<Jdb_log_3val_frame*>(ts);
		  Tb_entry_ke_reg *tb = Jdb_tbuf::new_entry<Tb_entry_ke_reg>();
		  str = regs->str();
		  tb->set(t, ip-1);
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
		    reinterpret_cast<Jdb_status_page_frame*>(ts);
		  regs->set(s->virt_to_phys(Mem_layout::Tbuf_ustatus_page));
		}
	      break;
	    case 6: // fiasco_timer_disable
	      // XXX: no longer do Timer_tick::disable(0);
	      break;
	    case 7: // fiasco_timer_enable
	      // XXX: no longer do Timer_tick::enable(0);
	      break;
            case 8: // fiasco_tbuf_log_binary()
              // interrupts are disabled in handle_slow_trap()
              Jdb_log_frame *regs = reinterpret_cast<Jdb_log_frame*>(ts);
              Tb_entry_ke_bin *tb =
                static_cast<Tb_entry_ke_bin*>(Jdb_tbuf::new_entry());
              str = regs->str();
              tb->set(t, ip-1);
              for (len=0; len < Tb_entry_ke_bin::SIZE; len++)
                tb->set_buf(len, s->peek(str++, user));
              regs->set_tb_entry(tb);
              Jdb_tbuf::commit_entry();
              break;
	    }
	  break;
	case 30:
#if 0
	  switch (ts->value2())
	    {
	    case 1: // fiasco_register_symbols
		{
		  Jdb_symbols_frame *regs =
		    reinterpret_cast<Jdb_symbols_frame*>(ts);
		  Jdb_dbinfo::set(Jdb_symbol::lookup(cxx::dyn_cast<Task*>(os->lookup(regs->task()))),
				  regs->addr(), regs->size());
		}
	      break;
	    case 2: // fiasco_register_lines
		{
		  Jdb_lines_frame *regs =
		    reinterpret_cast<Jdb_lines_frame*>(ts);
		  Jdb_dbinfo::set(Jdb_lines::lookup(regs->task()),
				  regs->addr(), regs->size());
		}
	      break;
	    }
#endif
	  break;
	default: // ko
	  if (todo < ' ')
	    return 0; // => Jdb

	  putchar(todo);
	  break;
	}
      break;
	}

    default:
      return 0; // => Jdb
    }

  return 1;
}
