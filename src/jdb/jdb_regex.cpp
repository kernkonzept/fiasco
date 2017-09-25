INTERFACE:

// ------------------------------------------------------------------------
INTERFACE [!jdb_regex]:

class Jdb_regex
{
public:
  static bool avail() { return false; }
};

// ------------------------------------------------------------------------
INTERFACE [jdb_regex]:

#include "initcalls.h"
#include "regex.h"

class Jdb_regex
{
private:
  static const unsigned heap_size = 64 * 1024;
  static char _init_done;

public:
  static bool avail() { return true; }

  static regex_t     _r;
  static regmatch_t  _matches[1];
};

// ------------------------------------------------------------------------
IMPLEMENTATION [!jdb_regex]:

PUBLIC static
int
Jdb_regex::start(const char *)
{ return 0; }

PUBLIC static
int
Jdb_regex::find(const char *, const char **, const char **)
{ return 0; }

// ------------------------------------------------------------------------
IMPLEMENTATION [jdb_regex]:

#include "config.h"
#include "jdb_module.h"
#include "kmem_alloc.h"
#include "panic.h"
#include "static_init.h"

char       Jdb_regex::_init_done;
regex_t    Jdb_regex::_r;
regmatch_t Jdb_regex::_matches[1];


STATIC_INITIALIZE_P(Jdb_regex, JDB_MODULE_INIT_PRIO);

PUBLIC static
void FIASCO_INIT
Jdb_regex::init()
{
  if (!_init_done)
    {
      char *heap = (char*)Kmem_alloc::allocator()->unaligned_alloc(heap_size);
      if (!heap)
	panic("No memory for regex heap");
      regex_init(heap, heap_size);
      _init_done = 1;
    }
}

PUBLIC static
int
Jdb_regex::start(const char *searchstr)
{
  // clear regex heap
  regex_reset();
  // compile expression
  return regcomp(&Jdb_regex::_r, searchstr, REG_EXTENDED) ? 0 : 1;
}

PUBLIC static
int
Jdb_regex::find(const char *buffer, const char **beg, const char **end)
{
  // execute expression
  int ret = regexec(&Jdb_regex::_r, buffer, 
		    sizeof(Jdb_regex::_matches)/sizeof(Jdb_regex::_matches[0]),
		    Jdb_regex::_matches, 0);

  if (ret == REG_NOMATCH)
    return 0;

  if (beg)
    *beg = buffer + Jdb_regex::_matches[0].rm_so;
  if (end)
    *end = buffer + Jdb_regex::_matches[0].rm_eo;
  return 1;
}

