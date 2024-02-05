
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "simple_malloc.h"
#include "paging_bits.h"
#include <capstone/capstone.h>
#include "disasm.h"

int
disasm_bytes(unsigned printlen, bool clreol, Jdb_address addr,
             [[maybe_unused]] bool show_intel_syntax,
             [[maybe_unused]] bool show_arm_thumb,
             Peek_task peek_task, Is_adp_mem is_adp_mem)
{
  int ret;
  static csh handle;
  if (handle == 0)
    {
      static cs_opt_mem mem =
        {
          simple_malloc,
          simple_calloc,
          simple_realloc,
          simple_free,
          vsnprintf
        };
      ret = cs_option(0, CS_OPT_MEM, reinterpret_cast<size_t>(&mem));
#if defined(CONFIG_IA32)
      ret = cs_open(CS_ARCH_X86, (cs_mode)(CS_MODE_32), &handle);
#elif defined(CONFIG_AMD64)
      ret = cs_open(CS_ARCH_X86, (cs_mode)(CS_MODE_64), &handle);
#elif defined(CONFIG_ARM) && defined(CONFIG_BIT32)
      auto syntax = static_cast<cs_mode>(CS_MODE_ARM|CS_MODE_LITTLE_ENDIAN);
      ret = cs_open(CS_ARCH_ARM, syntax, &handle);
#elif defined(CONFIG_ARM) && defined(CONFIG_BIT64)
      auto syntax = static_cast<cs_mode>(CS_MODE_ARM|CS_MODE_LITTLE_ENDIAN);
      ret = cs_open(show_arm_thumb ? CS_ARCH_ARM : CS_ARCH_ARM64, syntax, &handle);
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
#elif defined(CONFIG_RISCV) && defined(CONFIG_BIT32)
      auto syntax = static_cast<cs_mode>(CS_MODE_RISCV32|CS_MODE_LITTLE_ENDIAN);
      ret = cs_open(CS_ARCH_RISCV, syntax, &handle);
#elif defined(CONFIG_RISCV) && defined(CONFIG_BIT64)
      auto syntax = static_cast<cs_mode>(CS_MODE_RISCV64|CS_MODE_LITTLE_ENDIAN);
      ret = cs_open(CS_ARCH_RISCV, syntax, &handle);
#endif
      if (ret != CS_ERR_OK)
        handle = 0;
    }
  int size = 0;
  if (handle)
    {
#if defined(CONFIG_IA32) || defined(CONFIG_AMD64)
      static_cast<void>(
        cs_option(handle, CS_OPT_SYNTAX,
                  show_intel_syntax ? CS_OPT_SYNTAX_INTEL : CS_OPT_SYNTAX_ATT));
#elif defined(CONFIG_ARM) && defined(CONFIG_BIT32)
      size_t mode = static_cast<size_t>(show_arm_thumb
                                        ? CS_MODE_THUMB
                                        : static_cast<cs_mode>(0));
      static_cast<void>(cs_option(handle, CS_OPT_MODE, mode));
#endif
      cs_insn *insn = NULL;

      unsigned char code[16];
      unsigned width1 = Config::PAGE_SIZE - Pg::offset(addr.addr());
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
              if (printlen > 9)
                printf("%-7s %.*s%s\n",
                       insn[0].mnemonic, printlen - 9, insn[0].op_str,
                       clreol ? "\033[K" : "");
              size = insn->size;
              cs_free(insn, cnt);
            }
          else if (printlen > 9)
            printf("........ (%d)%s\n",
                   cs_errno(handle), clreol ? "\033[K" : "");
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
