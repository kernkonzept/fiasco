IMPLEMENTATION[ia32,amd64]:

#include <cstdio>
#include "simpleio.h"

#include "io.h"
#include "jdb_module.h"
#include "jdb.h"
#include "pci.h"
#include "pic.h"
#include "static_init.h"

/**
 * Private IA32-I/O module.
 */
class Io_m : public Jdb_module
{
public:
  Io_m() FIASCO_INIT;

private:
  static char porttype;

  struct Port_io_buf {
    unsigned adr;
    unsigned val;
  };

  struct Pci_port_buf {
    unsigned bus;
    unsigned dev;
    unsigned subdev;
    unsigned reg;
    unsigned val;
  };
  
  struct Irq_buf {
    unsigned irq;
  };

  union Input_buffer {
    Port_io_buf io;
    Pci_port_buf pci;
    Irq_buf irq;
  };

  static Input_buffer buf;
};

static Io_m io_m INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

char               Io_m::porttype;
Io_m::Input_buffer Io_m::buf;


PUBLIC
Jdb_module::Action_code
Io_m::action( int cmd, void *&args, char const *&fmt, int &)
{
  static char const *const port_in_fmt    = " addr=%8p";
  static char const *const port_out_fmt   = " addr=%8p, val=%8x";
  static char const *const pci_in_fmt     = 
    "\b\b\b\b\b\bPCI conf: bus=%2x, dev=%2x, subdev=%2x, reg=%2x";
  static char const *const pci_out_fmt    = 
    "\b\b\b\b\b\bPCI conf: bus=%2x, dev=%2x, subdev=%2x, reg=%2x, val=%8x";
  static char const *const ack_irq_fmt    = " ack IRQ=%2x";
  static char const *const mask_irq_fmt   = " mask IRQ=%2x";
  static char const *const unmask_irq_fmt = " unmask IRQ=%2x";

  unsigned answer = 0xffffffff;

  if(args != &buf)
    {
      args = &buf;
      if (cmd==0) // in
	{
	  switch (porttype)
	    {
	    case '1':
	    case '2':
	    case '4': fmt = port_in_fmt;    return EXTRA_INPUT;
	    case 'p': fmt = pci_in_fmt;     return EXTRA_INPUT;
	    default:
	      puts(" - unknown port type (must be 1,2,4 or p)");
	      return NOTHING;
	    }
	}
      else // out
	{
	  switch (porttype)
	    {
	    case '1':
	    case '2':
	    case '4': fmt = port_out_fmt;   return EXTRA_INPUT;
	    case 'p': fmt = pci_out_fmt;    return EXTRA_INPUT;
	    case 'a': fmt = ack_irq_fmt;    return EXTRA_INPUT;
	    case 'm': fmt = mask_irq_fmt;   return EXTRA_INPUT;
	    case 'u': fmt = unmask_irq_fmt; return EXTRA_INPUT;
	    default:
	      puts(" - unknown port type (must be 1,2,4,p,a,m or u)");
	      return NOTHING;
	    }
	}
    }
  else
    {
      switch (porttype)
	{
	case '1': // in/out 8bit
	case '2': // in/out 16bit
	case '4': // in/out 32bit
	  if (buf.io.adr == 0xffffffff)
    	    return NOTHING;
	  if(cmd==0) // in
	    putstr(" => ");
	  switch (porttype)
	    {
	    case '1':
	      if (cmd==0) // in
		{
		  if (buf.io.adr == Pic::MASTER_OCW)
		    printf("%02x (shadow of master-PIC register)\n",
			Jdb::pic_status & 0x0ff);
		  else if (buf.io.adr == Pic::SLAVES_OCW)
		    printf("%02x (shadow of slave-PIC register)\n",
			Jdb::pic_status >> 8);
		  else
		    {
		      answer = Io::in8(buf.io.adr);
		      printf("%02x\n", answer);
		    }
		}
	      else // out
		{
		  if (buf.io.adr == Pic::MASTER_OCW)
		    {
		      Jdb::pic_status = 
			(Jdb::pic_status & 0xff00) | buf.io.val;
		      putstr(" (PIC mask will be set on \"g\")");
	    	    }
    		  else if (buf.io.adr == Pic::SLAVES_OCW)
		    {
		      Jdb::pic_status =
			(Jdb::pic_status & 0x00ff) | (buf.io.val<<8);
		      putstr(" (PIC mask will be set on \"g\")");
		    }
		  else 
	      	    Io::out8(buf.io.val, buf.io.adr );
		}
    	      break;
	    case '2':
	      if(cmd==0) // in
		{
		  answer = Io::in16(buf.io.adr);
		  printf("%04x\n", answer);
		} 
	      else
		Io::out16(buf.io.val, buf.io.adr);
	      break;
	    case '4': 
	      if(cmd==0) // in
		{
		  answer = Io::in32(buf.io.adr);
		  printf("%08x\n", answer);
		}
	      else
		Io::out32(buf.io.val, buf.io.adr );
	      break;
	    }
	  if (cmd==1)
	    putchar('\n');
	  break;

	case 'p': // pci
	  if (cmd == 0)
	    printf(" => 0x%08x",
		   Pci::read_cfg(Pci::Cfg_addr(buf.pci.bus, buf.pci.dev, buf.pci.subdev,
		                               buf.pci.reg), Pci::Cfg_width::Long));
	  else
	    Pci::write_cfg(Pci::Cfg_addr(buf.pci.bus, buf.pci.dev, buf.pci.subdev,
	                                 buf.pci.reg), (Unsigned32)buf.pci.val);
	  putchar('\n');
	  break;

	case 'a': // manual acknowledge IRQ at pic
	  if (buf.irq.irq < 8)
	    Io::out8(0x60 + buf.irq.irq, Pic::MASTER_ICW);
	  else 
	    {
	      Io::out8(0x60 + (buf.irq.irq & 7), Pic::SLAVES_ICW);
	      Io::out8(0x60 + 2,                 Pic::MASTER_ICW);
	    }
	  putchar('\n');
	  break;

	case 'm': // manual mask IRQ
    	  Jdb::pic_status |= (1 << buf.irq.irq);
	  puts(" (PIC mask will be set on \"g\")");
	  break;

	case 'u': // manual unmask IRQ
	  Jdb::pic_status  &= ~(Unsigned16)(1 << buf.irq.irq);
	  puts(" (PIC mask will be set on \"g\")");
	  break;
	}
    }
  return NOTHING;
}

PUBLIC
int Io_m::num_cmds() const
{ 
  return 2;
}

PUBLIC
Jdb_module::Cmd const * Io_m::cmds() const
{
  static Cmd cs[] =
    { 
	{ 0, "i", "in", " type:%c",
  	  "i{1|2|4|p}<num>\tin port",
	  &porttype },
        { 1, "o", "out", " type:%c",
	  "o{1|2|4|a|u|m}<num>\tout port, ack/(un)mask/ack irq",
	  &porttype },
    };
  return cs;
}

IMPLEMENT
Io_m::Io_m()
  : Jdb_module("INFO")
{}

