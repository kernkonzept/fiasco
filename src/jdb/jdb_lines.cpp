INTERFACE:

#include "l4_types.h"
class Space;

class Jdb_lines_line
{
public:
  unsigned long  addr;
  unsigned short line;
} __attribute__((packed));

class Jdb_lines_info
{
private:
  Address  _virt;
  size_t   _size;
  Address  _beg_line;
  Address  _end_line;
};

class Jdb_lines
{
public:
  enum
  {
    // don't allow more than 2048 tasks to register their lines to save space
    Max_tasks = 2048,
  };

private:
  static Jdb_lines_info *_task_lines;
};


IMPLEMENTATION:

#include <cstdio>
#include <cstring>
#include "panic.h"
#include "warn.h"

Jdb_lines_info *Jdb_lines::_task_lines;

PUBLIC static
void
Jdb_lines::init (void *mem, Mword pages)
{
  if (!mem || pages < (Max_tasks*sizeof(Jdb_lines_info))>>Config::PAGE_SHIFT)
    panic("No memory for lines");

  _task_lines = (Jdb_lines_info*)mem;
  memset(_task_lines, 0, Max_tasks * sizeof(Jdb_lines_info));
}

PUBLIC inline NOEXPORT
Jdb_lines_line*
Jdb_lines_info::lin ()
{
  return (Jdb_lines_line*)_virt;
}

PUBLIC inline NOEXPORT
const char*
Jdb_lines_info::str ()
{
  return (const char*)_virt;
}

PUBLIC inline
void
Jdb_lines_info::get (Address &virt, size_t &size)
{
  virt = _virt;
  size = _size;
}

PUBLIC inline
void
Jdb_lines_info::reset ()
{
  _virt = 0;
}

PUBLIC inline NOEXPORT
bool
Jdb_lines_info::in_range (Address addr)
{
  return _virt != 0 && _beg_line <= addr && _end_line >= addr;
}

// register lines for a specific task (called by user mode application)
PUBLIC
bool
Jdb_lines_info::set (Address virt, size_t size)
{
  _virt = virt;
  _size = size;

  Address min_addr = 0xffffffff, max_addr = 0;
  Jdb_lines_line *l;

  // search lines with lowest / highest address
  for (l = lin(); l->addr || l->line; l++)
    {
      if (l->line < 0xfffd)
	{
	  Address addr = l->addr;
	  if (addr < min_addr)
	    min_addr = addr;
	  if (addr > max_addr)
	    max_addr = addr;
	}
    }
  
  _beg_line = min_addr;
  _end_line = max_addr;

  return true;
}

PUBLIC static
Jdb_lines_info*
Jdb_lines::lookup (Space * task)
{
  (void)task;
#if 0
  if (task >= Max_tasks)
    {
      WARN ("register_lines: task value #%x out of range (0-%x)\n",
	    task, Max_tasks-1);
      return 0;
    }

  return _task_lines + task;
#endif
  return 0;
}

IMPLEMENTATION[!arm]:

// search line name that matches for a specific address
PUBLIC static
bool
Jdb_lines::match_addr_to_line(Address addr, Space * task,
			      char *line, unsigned line_size,
			      int show_header_files)
{
  (void)addr; (void)task; (void)line; (void)line_size; (void)show_header_files;
#if 0
  if (task >= Max_tasks)
    return false;

  if (! _task_lines[task].in_range (addr))
    return false;

  int show_file = 1;
  Jdb_lines_line *lin = _task_lines[task].lin();
  const char *dir="", *file="", *str = _task_lines[task].str();
      
  for (; lin->addr || lin->line; lin++)
    {
      if (lin->line == 0xfffe)
	{
	  // this is a directory entry
	  dir = str + lin->addr;
	}
      else if (lin->line == 0xffff)
	{
	  // this is a file name entry
	  file = str + lin->addr;
	  show_file = 1;
	}
      else if (lin->line == 0xfffd)
	{
	  // this is a header file name entry
	  file = str + lin->addr;
	  show_file = show_header_files;
	}
      else if ((lin->addr == addr) && show_file)
	{
	  // this is a line
	  sprint_line(dir, file, lin->line, line, line_size);
	  return true;
	}
    }
#endif
  return false;
}

// search line name that matches for a specific address
PUBLIC static
bool
Jdb_lines::match_addr_to_line_fuzzy(Address *addr_ptr, Space * task,
				    char *line, unsigned line_size,
				    int show_header_files)
{
  (void)addr_ptr; (void)task; (void)line; (void)line_size; (void)show_header_files;
#if 0
  if (task >= Max_tasks)
    return false;

  if (! _task_lines[task].in_range (*addr_ptr))
    return false;

  int show_file = 1;
  Address best_addr = (Address)-1, best_line = 0;
  Jdb_lines_line *lin = _task_lines[task].lin();
  const char *dir="", *file="", *best_dir=0, *best_file=0;
  const char *str=_task_lines[task].str();

  for (; lin->addr || lin->line; lin++)
    {
      if (lin->line == 0xfffe)
	{
	  // this is a directory entry
	  dir = str + lin->addr;
	}
      else if (lin->line == 0xffff)
	{
	  // this is a file name entry
	  file = str + lin->addr;
	  show_file = 1;
	}
      else if (lin->line == 0xfffd)
	{
	  // this is a header file name entry
	  file = str + lin->addr;
	  show_file = show_header_files;
	}
      else if ((lin->addr <= *addr_ptr)
	       && (*addr_ptr-lin->addr < best_addr)
	       && show_file)
	{
     	  best_addr = *addr_ptr-lin->addr;
	  best_dir  = dir;
	  best_file = file;
	  best_line = lin->line;
	}
    }

  if (best_addr < (Address)-1)
    {
      sprint_line(best_dir, best_file, best_line, line, line_size);
      *addr_ptr -= best_addr;
      return true;
    }
#endif
  return false;
}

// truncate string if its length exceeds strsize and create line
static
void
Jdb_lines::sprint_line(const char *dir, const char *fname, unsigned line,
		       char *str, size_t strsize)
{
  unsigned d = strlen(dir);
  unsigned f = strlen(fname);

  if (!str)
    return;

  if (fname[0]=='/')
    d=0;

  // handle fname in form of ../../../../foo/bar.h
  while (d>1 && f>2 && fname[0]=='.' && fname[1]=='.' && fname[2]=='/')
    {
      // cut last directory
      while ((d>1) && (dir[d-2]!='/'))
	d--;
      d--;
      // cut first ../ directory of fname
      fname+=3;
      f-=3;
    }

  if (strsize < 10)
    {
      *str = '\0';
      return;
    }

  strsize -= 6;	// for line number
  
  if ((d+f) > strsize)
    {
      // abbreviate line in a sane way
      memcpy(str, "... ", 4);
      str     += 4;
      strsize -= 4;

      if (strsize > f)
	{
	  unsigned dp = strsize-f;
	  memcpy(str, dir+d-dp, dp);
	  str     += dp;
	  strsize -= dp;
	}
      memcpy(str, fname+f-strsize, strsize);
      str += strsize;
    }
  else
    {
      memcpy(str, dir, d);
      memcpy(str+d, fname, f);
      str += d+f;
    }

  snprintf(str, strsize, ":%u", line);
}

IMPLEMENTATION[arm]:

PUBLIC static
bool
Jdb_lines::match_addr_to_line(Address , Space * ,
			      char * , unsigned , int )
{
  return 0;
}

