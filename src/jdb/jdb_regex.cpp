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
public:
  static bool avail() { return true; }

  regex_t     _r;
  regmatch_t  _matches[1];
  bool        _active;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [!jdb_regex]:

PUBLIC
bool
Jdb_regex::start(const char *)
{ return true; }

PUBLIC
bool
Jdb_regex::find(const char *, const char **, const char **)
{ return false; }

// ------------------------------------------------------------------------
IMPLEMENTATION [jdb_regex]:

#include <cstring>

#include "config.h"
#include "jdb_module.h"
#include "kmem_alloc.h"
#include "panic.h"
#include "simple_malloc.h"

PUBLIC
Jdb_regex::Jdb_regex()
  : _active(false)
{
}

PUBLIC
Jdb_regex::~Jdb_regex()
{
  if (_active)
    regfree(&_r);
}

PUBLIC
bool
Jdb_regex::start(const char *searchstr)
{
  if (!searchstr || !*searchstr)
    return true;

  finish();

  // compile expression
  if (regcomp(&_r, searchstr, REG_EXTENDED) == 0)
    return (_active = true);

  return false;
}

PUBLIC
void
Jdb_regex::finish()
{
  if (_active)
    regfree(&_r);
  memset(_matches, 0, sizeof(_matches));
  _active = false;
}

PUBLIC
bool
Jdb_regex::find(const char *buffer, const char **beg, const char **end)
{
  if (!_active)
    return false;

  // execute expression
  int ret = regexec(&_r, buffer,
		    sizeof(_matches)/sizeof(_matches[0]), _matches, 0);

  if (ret == REG_NOMATCH)
    return false;

  if (beg)
    *beg = buffer + _matches[0].rm_so;
  if (end)
    *end = buffer + _matches[0].rm_eo;
  return true;
}
