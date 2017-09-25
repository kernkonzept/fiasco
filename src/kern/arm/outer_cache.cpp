INTERFACE:

#include "types.h"

class Outer_cache
{
public:
  static void platform_init_post();

  static void invalidate();
  static void invalidate(Address phys, bool sync = true);
  static void invalidate(Address start_phys, Address end_phys, bool do_sync = true);

  static void clean();
  static void clean(Address phys, bool do_sync = true);
  static void clean(Address start_phys, Address end_phys, bool do_sync = true);

  static void flush();
  static void flush(Address phys, bool do_sync = true);
  static void flush(Address start_phys, Address end_phys, bool do_sync = true);

  static void sync();
};


// ------------------------------------------------------------------------
IMPLEMENTATION:

IMPLEMENT_DEFAULT inline
void
Outer_cache::platform_init_post()
{}


// ------------------------------------------------------------------------
IMPLEMENTATION [!outer_cache]:

IMPLEMENT inline
void
Outer_cache::invalidate()
{}

IMPLEMENT inline
void
Outer_cache::invalidate(Address, bool)
{}

IMPLEMENT inline
void
Outer_cache::clean()
{}

IMPLEMENT inline
void
Outer_cache::clean(Address, bool)
{}

IMPLEMENT inline
void
Outer_cache::flush()
{}

IMPLEMENT inline
void
Outer_cache::flush(Address, bool)
{}

IMPLEMENT inline
void
Outer_cache::sync()
{}

IMPLEMENT inline
void
Outer_cache::invalidate(Address, Address, bool)
{}

IMPLEMENT inline
void
Outer_cache::clean(Address, Address, bool)
{}

IMPLEMENT inline
void
Outer_cache::flush(Address, Address, bool)
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [outer_cache]:

IMPLEMENT inline
void
Outer_cache::invalidate(Address start, Address end, bool do_sync)
{
  if (start & Cache_line_mask)
    {
      flush(start, false);
      start += Cache_line_size;
    }
  if (end & Cache_line_mask)
    {
      flush(end, false);
      end &= ~Cache_line_mask;
    }

  for (Address a = start & ~Cache_line_mask; a < end; a += Cache_line_size)
    invalidate(a, false);

  if (do_sync)
    sync();
}

IMPLEMENT inline
void
Outer_cache::clean(Address start, Address end, bool do_sync)
{
  for (Address a = start & ~Cache_line_mask;
       a < end; a += Cache_line_size)
    clean(a, false);
  if (do_sync)
    sync();
}

IMPLEMENT inline
void
Outer_cache::flush(Address start, Address end, bool do_sync)
{
  for (Address a = start & ~Cache_line_mask;
       a < end; a += Cache_line_size)
    flush(a, false);
  if (do_sync)
    sync();
}
