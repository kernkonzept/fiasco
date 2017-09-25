INTERFACE:

#include <globalconfig.h>
#include "l4_types.h"

class Space;

class Jdb_symbol_info
{
private:
  Address  _virt;
  size_t   _size;
  Address  _beg_sym;
  Address  _end_sym;
};

class Jdb_symbol
{
public:
  enum
  {
    // don't allow more than 2048 tasks to register their symbols to save space
    Max_tasks = 2048,
#ifdef CONFIG_BIT32
    Digits = 8,
    Start  = 11,
#else
    Digits = 16,
    Start  = 19,
#endif
  };

private:
  static Jdb_symbol_info *_task_symbols;
};


IMPLEMENTATION:

#include <cstdio>
#include <cstring>
#include "config.h"
#include "mem_layout.h"
#include "panic.h"
#include "warn.h"

Jdb_symbol_info *Jdb_symbol::_task_symbols;

PUBLIC static
void
Jdb_symbol::init (void *mem, Mword pages)
{
  if (!mem || pages < (Max_tasks*sizeof(Jdb_symbol_info))>>Config::PAGE_SHIFT)
    panic("No memory for symbols");

  _task_symbols = (Jdb_symbol_info*)mem;
  memset(_task_symbols, 0, Max_tasks * sizeof(Jdb_symbol_info));
}

PUBLIC inline NOEXPORT
char*
Jdb_symbol_info::str ()
{ 
  return (char*) _virt;
}

PUBLIC inline
void
Jdb_symbol_info::get (Address &virt, size_t &size)
{
  virt = _virt;
  size = _size;
}

PUBLIC inline
void
Jdb_symbol_info::reset ()
{
  _virt = 0;
}

PUBLIC inline NOEXPORT
bool
Jdb_symbol_info::in_range (Address addr)
{
  return _virt != 0 && _beg_sym <= addr && _end_sym >= addr;
}

// read address from current position and return its value
PRIVATE static inline NOEXPORT
Address
Jdb_symbol_info::string_to_addr (const char *symstr)
{
  Address addr = 0;

  for (int i=0; i< Jdb_symbol::Digits; i++)
    {
      switch (Address c = *symstr++)
	{
	case 'a': case 'b': case 'c':
	case 'd': case 'e': case 'f':
	  c -= 'a' - '9' - 1;
	case '0': case '1': case '2':
	case '3': case '4': case '5':
	case '6': case '7': case '8':
	case '9':
	  c -= '0';
	  addr <<= 4;
	  addr += c;
	}
    }

  return addr;
}

// Optimize symbol table to speed up address-to-symbol search.
// Replace stringified address (8/16 characters) by 2/4 dwords of 32/64 bit:
// The symbol value and the absolute address of the next symbol (or 0 if end
// of list)
PRIVATE
bool
Jdb_symbol_info::transform ()
{
  char *symstr = str();
  size_t     s = _size;

  for (int line=0; ; line++)
    {
      char *sym = symstr;

      // set symstr to the end of the current line
      while (s && (*symstr != '\0') && (*symstr != '\n'))
	{
	  symstr++;
	  s--;
	}

      if (!s || (symstr-sym < Jdb_symbol::Start))
	{
	  if (line == 0)
	    {
	      WARN("Invalid symbol table at " L4_PTR_FMT
		   " -- disabling symbols.\n", (Address)sym);
	      return false;
	    }

	  ((Address*)sym)[0] = 0; // terminate list
	  ((Address*)sym)[1] = 0; // terminate list
	  if (s)
	    *symstr = '\0';       // terminate string
	  return true;
	}

      // write binary value of symbol string
      Address addr = string_to_addr(sym);
      Address *sym_addr = (Address*)sym;
      *sym_addr = addr;

      sym += sizeof(Address);
      sym_addr = (Address*)sym;

      if (!*symstr || !*(symstr+1))
	{
	  *symstr = '\0'; // terminate symbol
	  *sym_addr = 0;  // terminate list
	  return true;
	}

      // terminate current symbol -- overwrite '\n'
      *symstr = '\0';

      // write link address to next symbol
      *sym_addr = (Address)++symstr;
      s--;
    }
}

// register symbols for a specific task (called by user mode application)
PUBLIC
bool
Jdb_symbol_info::set (Address virt, size_t size)
{
  _virt = virt;
  _size = size;

  if (! transform())
    return false;

  if (this == Jdb_symbol::lookup(0))
    {
      // kernel symbols are special
      Address sym_start, sym_end;

      if (  !(sym_start = Jdb_symbol::match_symbol_to_addr("_start", false, 0))
    	  ||!(sym_end   = Jdb_symbol::match_symbol_to_addr("_end", false, 0)))
	{
	  WARN ("Missing \"_start\" or \"_end\" in kernel symbols "
		"-- disabling kernel symbols\n");
	  return false;
	}

      if (sym_end != (Address)&Mem_layout::end)
	{
	  WARN("Fiasco symbol table does not match kernel "
	       "-- disabling kernel symbols\n");
	  return false;
	}

      _beg_sym = sym_start;
      _end_sym = sym_end;
    }
  else
    {
      Address min_addr = ~0UL, max_addr = 0;
      const char *sym;

      for (sym = str(); sym; sym = *((const char**)sym+1))
	{
	  Address addr = *(Address*)sym;
	  if (addr < min_addr)
	    min_addr = addr;
	  if (addr > max_addr)
	    max_addr = addr;
	}
      
      _beg_sym = min_addr;
      _end_sym = max_addr;
    }

  return true;
}

PUBLIC static
Jdb_symbol_info*
Jdb_symbol::lookup (Space *task)
{
  (void)task;
#if 0
  if (task >= Max_tasks)
    {
      WARN ("register_symbols: task value #%x out of range (0-%x).\n",
	    task, Max_tasks-1);
      return 0;
    }
  return _task_symbols + task;
#endif
  return 0;
}

// search symbol in task's symbols, return pointer to symbol name
static
const char*
Jdb_symbol::match_symbol (const char *symbol, bool search_instr, Space *task)
{
  (void)symbol; (void)search_instr; (void)task;
#if 0
  const char *sym, *symnext;

  if (task >= Max_tasks)
    return 0;

  // walk through list of symbols
  for (sym=_task_symbols[task].str(); sym; sym=symnext)
    {
      symnext = *((const char**)sym+1);
      
      // search symbol
      if (  ( search_instr && ( strstr(sym+Jdb_symbol::Start, symbol) == 
	      				sym+Jdb_symbol::Start))
	  ||(!search_instr && (!strcmp(sym+Jdb_symbol::Start, symbol))))
	return sym;
    }
#endif
  return 0;
}

// Search a symbol in symbols of a specific task. If the symbol
// was not found, search in kernel symbols too (if exists)
PUBLIC static
Address
Jdb_symbol::match_symbol_to_addr (const char *symbol, bool search_instr,
				  Space *task)
{
  (void)symbol; (void)search_instr; (void)task;
#if 0
  const char *sym;

  if (task >= Max_tasks)
    return 0;

  for (;;)
    {
      if (   !_task_symbols[task].str()
	  || !(sym = match_symbol(symbol, search_instr, task)))
	{
	  // no symbols for task or symbol not found in symbols
	  if (task != 0)
	    {
	      task = 0; // not yet searched in kernel symbols, do it now
	      continue;
	    }
	  return 0; // already searched in kernel symbols so we fail
	}

      return *(Address*)sym;
    }
#endif
  return 0;
}

// try to search a possible symbol completion
PUBLIC static
bool
Jdb_symbol::complete_symbol (char *symbol, unsigned size, Space *task)
{
  (void)symbol; (void)size; (void)task;
#if 0
  unsigned equal_len = 0xffffffff, symbol_len = strlen(symbol);
  const char *sym, *symnext, *equal_sym = 0;

  if (symbol_len >= --size)
    return false;

repeat:
  if (task < Max_tasks && _task_symbols[task].str() != 0)
    {
      // walk through list of symbols
      for (sym=_task_symbols[task].str(); sym; sym=symnext)
	{
	  symnext = *((const char**)sym+1);
	  sym += Start;
      
	  // search symbol
	  if (strstr(sym, symbol) == sym)
	    {
	      if (!equal_sym)
		{
		  equal_len = strlen(sym);
		  equal_sym = sym;
		}
	      else
		{
		  unsigned c = 0;
		  const char *s1 = sym, *s2 = equal_sym;
	  
		  while (*s1++ == *s2++)
		    c++;
	      
		  if (c < equal_len)
		    {
		      equal_len = c;
		      equal_sym = sym;
		    }
		}
	    }
	}
    }

  if (equal_sym)
    {
      if (equal_len > symbol_len)
	{
	  snprintf(symbol, size < equal_len+1 ? size : equal_len+1, "%s", equal_sym);
	  return true;
	}
    }
  else
    {
      // no completion found or no symbols
      if (task != 0)
	{
	  // repeat search with kernel symbols
	  task = 0;
	  goto repeat;
	}

      int c;
      if (   (symbol_len < 1)
	  || (((c = symbol[symbol_len-1]) != '?') && (c != '*')))
	{
	  symbol[symbol_len++] = '?';
	  symbol[symbol_len]   = '\0';
	  return true;
	}
    }
#endif
  return false;
}

IMPLEMENTATION[!arm]:

// search symbol name that matches for a specific address
PUBLIC static
const char*
Jdb_symbol::match_addr_to_symbol (Address addr, Space *task)
{
  (void)addr; (void)task;
#if 0
  if (task >= Max_tasks)
    return 0;

  if (!_task_symbols || !_task_symbols[task].in_range (addr))
    return 0;

  const char *sym;

  for (sym = _task_symbols[task].str(); sym; sym = *((const char**)sym+1))
    {
      if (   (*(Address*)sym == addr)
	  && (memcmp((void*)(sym+Start), "Letext", 6))		// ignore
	  && (memcmp((void*)(sym+Start), "patch_log_", 10))	// ignore
	 )
	return sym+Start;
    }
#endif
  return 0;
}

IMPLEMENTATION:

// search last symbol before eip
PUBLIC static
bool
Jdb_symbol::match_addr_to_symbol_fuzzy (Address *addr_ptr, Space *task,
				        char *t_symbol, int s_symbol)
{
  (void)addr_ptr; (void)task; (void)t_symbol; (void)s_symbol;
#if 0
  if (task >= Max_tasks)
    return false;

  if (!_task_symbols || !_task_symbols[task].in_range (*addr_ptr))
    return false;

  const char *sym, *max_sym = 0;
  Address max_addr = 0;

  for (sym = _task_symbols[task].str(); sym; sym = *((const char**)sym+1))
    {
      Address addr = *(Address*)sym;
      if (   addr > max_addr && addr <= *addr_ptr
	  && (memcmp((void*)(sym+Start), "Letext", 6))		// ignore
	  && (memcmp((void*)(sym+Start), "patch_log_", 10))	// ignore
	 )
	{
	  max_addr = addr;
	  max_sym  = sym;
	}
    }
      
  if (max_sym)
    {
      const char *t = max_sym + Start;

      while (*t != '\n' && *t != '\0' && --s_symbol)
	{
	  *t_symbol++ = *t++;

	  // print functions with arguments as ()
	  if (*(t-1) == '(')
	    while (*t != ')' && *t != '\n' && *t != '\0')
	      t++;
	}

      // terminate string
      *t_symbol = '\0';

      *addr_ptr = max_addr;
      return true;
    }
#endif
  return false;
}

IMPLEMENTATION[arm]:

PUBLIC static
const char*
Jdb_symbol::match_addr_to_symbol (Address , Space *)
{
  return 0;
}
