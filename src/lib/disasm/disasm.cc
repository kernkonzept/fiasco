
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "simple_malloc.h"
#include <capstone/capstone.h>
#include "disasm.h"

#if 0
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
#endif

int
disasm_bytes(char *buffer, unsigned len, Jdb_address addr,
             int show_intel_syntax,
             Peek_task peek_task, Is_adp_mem is_adp_mem)
{
  // symbols / lines not supported ATM anyway
#if 0
  /* test for special L4 opcodes */
  if (my_get_data(addr) == 0xcc && (len = special_l4_ops(addr+1)))
    return len;

  /* one step back for special L4 opcodes */
  if (my_get_data(addr-1) == 0xcc && (len = special_l4_ops(addr)))
    return len-1;
#endif

  (void)show_intel_syntax;

  int ret;
  static csh handle;
  if (handle == (csh)0)
    {
      static cs_opt_mem mem =
        {
          simple_malloc,
          simple_calloc,
          simple_realloc,
          simple_free,
          vsnprintf
        };
      ret = cs_option((csh)NULL, CS_OPT_MEM, (size_t)&mem);
#if defined(CONFIG_IA32)
      ret = cs_open(CS_ARCH_X86, (cs_mode)(CS_MODE_32), &handle);
#elif defined(CONFIG_AMD64)
      ret = cs_open(CS_ARCH_X86, (cs_mode)(CS_MODE_64), &handle);
#elif defined(CONFIG_ARM) && defined(CONFIG_BIT32)
      ret = cs_open(CS_ARCH_ARM, (cs_mode)(CS_MODE_ARM|CS_MODE_LITTLE_ENDIAN), &handle);
#elif defined(CONFIG_ARM) && defined(CONFIG_BIT64)
      ret = cs_open(CS_ARCH_ARM64, (cs_mode)(CS_MODE_ARM|CS_MODE_LITTLE_ENDIAN), &handle);
#elif defined(CONFIG_MIPS)
      ret = cs_open(CS_ARCH_MIPS,
                    (cs_mode)(
# if defined(CONFIG_BIT32)
                              CS_MODE_MIPS32
# else
                              CS_MODE_MIPS64
# endif
                              |
# if defined(CONFIG_MIPS_LITTLE_ENDIAN)
                              CS_MODE_LITTLE_ENDIAN
# else
                              CS_MODE_BIG_ENDIAN
# endif
                             ), &handle);
#endif
    }
  if (buffer)
    *buffer = '\0';
  int size = 0;
  if (handle)
    {
#if defined(CONFIG_IA32) || defined(CONFIG_AMD64)
      ret = cs_option(handle, CS_OPT_SYNTAX,
                      show_intel_syntax ? CS_OPT_SYNTAX_INTEL : CS_OPT_SYNTAX_ATT);
#endif
      cs_insn *insn = NULL;

      unsigned char code[16];
      unsigned width1 = Config::PAGE_SIZE - (addr.addr() & ~Config::PAGE_MASK);
      if (width1 > sizeof(code))
        width1 = sizeof(code);
      if (is_adp_mem(addr) || peek_task(addr, &code, width1) < 0)
        size = -1;
      else
        {
          unsigned width2 = sizeof(code) - width1;
          // if fetching the 2nd part fails just hope that the first part is enough
          if (width2
              && (is_adp_mem(addr + width1)
                  || peek_task(addr + width1, &code[width1], width2) < 0))
            memset(&code[width1], 0, width2);
          int cnt = cs_disasm(handle, code, sizeof(code), addr.addr(), 1, &insn);
          if (cnt)
            {
              if (buffer)
                snprintf(buffer, len, "%-7s %s", insn[0].mnemonic, insn[0].op_str);
              size = insn->size;
              cs_free(insn, cnt);
            }
          else if (buffer)
            snprintf(buffer, len, "........ (%d)", cs_errno(handle));
        }
    }

  if (!size)
    {
#if defined(CONFIG_ARM)
      size = 4;
#else
      size = 1;
#endif
    }

  return size;
}
