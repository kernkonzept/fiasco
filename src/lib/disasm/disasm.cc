
#include <malloc.h>
#include <stdarg.h>
#include <string.h>

#include "include/dis-asm.h"
#include "disasm.h"

/* local variables */
static char *out_buf;
static int out_len;
static int use_syms;
static Space *dis_task;
static disassemble_info dis_info;
static Peek_task dis_peek_task;
static Get_symbol dis_get_symbol;

extern int print_insn_i386 PARAMS ((bfd_vma, disassemble_info*));

/* read <length> bytes starting at memaddr */
static int
my_read_memory(bfd_vma memaddr, bfd_byte *myaddr, unsigned length,
	       disassemble_info *info __attribute__ ((unused)))
{
  unsigned i;

  for (i=0; i<length; i++)
    {
      Mword value;
      if (dis_peek_task(memaddr+i, dis_task, &value, 1) == -1)
	return -1;
      *myaddr++ = (bfd_byte)value;
    }

  return 0;
}

/* XXX return byte without range check */
static unsigned char
my_get_data(bfd_vma memaddr)
{
  Mword value;
  if (dis_peek_task(memaddr, dis_task, &value, 1) == -1)
    return -1;

  return value;
}

/* print address (using symbols if possible) */
static void
my_print_address(bfd_vma memaddr, disassemble_info *info)
{
  const char *symbol;

  if (use_syms && dis_get_symbol
      && (symbol = dis_get_symbol(memaddr, dis_task)))
    {
      unsigned i;
      char buf[48];
      const char *s;
      
      buf[0] = ' ';
      buf[1] = '<';
      for (i=2, s=symbol; i<(sizeof(buf)-3); )
	{
	  if (*s=='\0' || *s=='\n')
	    break;
	  buf[i++] = *s++;
	  if (*(s-1)=='(')
	    while ((*s != ')') && (*s != '\n') && (*s != '\0'))
	      s++;
	}
      buf[i++] = '>';
      buf[i++] = '\0';

      (*info->fprintf_func)(info->stream, "%s", buf);
    }
}

static int
my_printf(void* stream __attribute__ ((unused)), const char *format, ...)
{
  if (out_len)
    {
      int len;
      va_list list;
  
      va_start(list, format);
      len = vsnprintf(out_buf, out_len, format, list);
      if (len >= out_len)
	len = out_len - 1;
      out_buf += len;
      out_len -= len;
      va_end(list);
    }
  
  return 0;
}

static void
my_putchar(int c)
{
  if (out_len)
    {
      out_len--;
      *out_buf++ = c;
    }
}

/* check for special L4 int3-opcodes */
static int
special_l4_ops(bfd_vma memaddr)
{
  int len, bytes, i;
  const char *op;
  bfd_vma str, s;
  
  switch (my_get_data(memaddr))
    {
    case 0xeb:
      op  = "enter_kdebug";
      len = my_get_data(memaddr+1);
      str = memaddr+2;
      bytes = 3+len;
      goto printstr;
    case 0x90:
      if (my_get_data(memaddr+1) != 0xeb)
	break;
      op  = "kd_display";
      len = my_get_data(memaddr+2);
      str = memaddr+3;
      bytes = 4+len;
    printstr:
      /* do a quick test if it is really an int3-str function by
       * analyzing the bytes we shall display. */
      for (i=len, s=str; i--; )
	if (my_get_data(s++) > 126)
	  return 0;
      /* test well done */
      my_printf(0, "<%s (\"", op);
      if ((out_len > 2) && (len > 0))
	{
	  out_len -= 3;
     	  if (out_len > len)
	    out_len = len;
	  /* do not use my_printf here because the string
	   * can contain special characters (e.g. tabs) which 
	   * we do not want to display */
	  while (out_len)
	    {
	      unsigned char c = my_get_data(str++);
	      my_putchar((c<' ') ? ' ' : c);
	    }
	  out_len += 3;
	}
      my_printf(0, "\")>");
      return bytes;
    case 0x3c:
      op = NULL;
      switch (my_get_data(memaddr+1))
	{
	case 0: op = "outchar (%al)";  break;
	case 2: op = "outstring (*%eax)"; break;
	case 5: op = "outhex32 (%eax)"; break;
	case 6: op = "outhex20 (%eax)"; break;
	case 7: op = "outhex16 (%eax)"; break;
	case 8: op = "outhex12 (%eax)"; break;
	case 9: op = "outhex8 (%al)"; break;
	case 11: op = "outdec (%eax)"; break;
	case 13: op = "%al = inchar ()"; break;
	case 24: op = "fiasco_start_profile()"; break;
	case 25: op = "fiasco_stop_and_dump()"; break;
	case 26: op = "fiasco_stop_profile()"; break;
	case 29: op = "fiasco_tbuf (%eax)"; break;
	case 30: op = "fiasco_register (%eax, %ecx)"; break;
	}
      if (op)
	my_printf(0, "<%s>", op);
      else if (my_get_data(memaddr+1) >= ' ')
	my_printf(0, "<ko ('%c')>", my_get_data(memaddr+1));
      else break;
      return 3;
    }

  return 0;
}

/* WARNING: This function is not reentrant because it accesses some
 * global static variables (out_buf, out_len, dis_task, ...) */
unsigned int
disasm_bytes(char *buffer, unsigned len, Address addr,
	     Space *task, int show_symbols, int show_intel_syntax,
	     Peek_task peek_task, Get_symbol get_symbol)
{
  use_syms       = show_symbols;
  out_buf        = buffer;
  out_len        = len;
  dis_task       = task;
  dis_peek_task  = peek_task;
  dis_get_symbol = get_symbol;

  /* terminate string */
  if (out_len)
    out_buf[--out_len] = '\0';

  /* test for special L4 opcodes */
  if (my_get_data(addr) == 0xcc && (len = special_l4_ops(addr+1)))
    return len;

  /* one step back for special L4 opcodes */
  if (my_get_data(addr-1) == 0xcc && (len = special_l4_ops(addr)))
    return len-1;

  INIT_DISASSEMBLE_INFO(dis_info, NULL, my_printf);
  
  dis_info.print_address_func = my_print_address;
  dis_info.read_memory_func = my_read_memory;
  dis_info.buffer = (bfd_byte*)addr;
  dis_info.buffer_length = 99; /* XXX */
  dis_info.buffer_vma = addr;
#if defined CONFIG_ARM
  dis_info.mach = bfd_mach_arm_5;
  return print_insn_little_arm (addr, &dis_info);
#elif defined CONFIG_PPC32
  dis_info.mach = bfd_mach_ppc_ec603e;
  return print_insn_big_powerpc(addr, &dis_info);
#elif defined CONFIG_IA32
  dis_info.mach = show_intel_syntax ? bfd_mach_i386_i386_intel_syntax
				    : bfd_mach_i386_i386;
  return print_insn_i386 (addr, &dis_info);
#elif defined CONFIG_AMD64
  dis_info.mach = show_intel_syntax ? bfd_mach_x86_64_intel_syntax
				    : bfd_mach_x86_64;
  return print_insn_i386 (addr, &dis_info);
#elif defined CONFIG_SPARC
  dis_info.mach = bfd_mach_sparc_v8plusa;
  return print_insn_sparc(addr, &dis_info);
#else
#error Unknown architecture
#endif
  (void)show_intel_syntax;
}
