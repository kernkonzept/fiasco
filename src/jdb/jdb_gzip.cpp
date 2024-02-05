IMPLEMENTATION:

#include <cstring>
#include <cstdio>

#include "config.h"
#include "console.h"
#include "gzip.h"
#include "kernel_console.h"
#include "kmem_alloc.h"
#include "panic.h"
#include "static_init.h"
#include "paging_bits.h"

class Jdb_gzip : public Console
{
  static const unsigned heap_pages = 28;
  char   active;
  char   init_done;
  static Console *uart;
};

Console *Jdb_gzip::uart;

Jdb_gzip::Jdb_gzip() : Console(DISABLED)
{
  char *heap = static_cast<char*>(Kmem_alloc::allocator()->
                                    alloc(Bytes(Pg::size(heap_pages))));
  if (!heap)
    panic("No memory for gzip heap");
  gz_init(heap, Pg::size(heap_pages), raw_write);
}

static void
Jdb_gzip::raw_write(const char *s, size_t len)
{
  if (uart)
    uart->write(s, len);
}

PUBLIC inline NOEXPORT
void
Jdb_gzip::enable()
{
  if (!init_done)
    {
      uart = Kconsole::console()->find_console(Console::UART);
      init_done = 1;
    }

  gz_open("jdb.gz");
  active = 1;
}

PUBLIC inline NOEXPORT
void
Jdb_gzip::disable()
{
  if (active)
    {
      gz_close();
      active = 0;
    }
}

PUBLIC void
Jdb_gzip::state(Mword new_state) override
{
  if ((_state ^ new_state) & OUTENABLED)
    {
      if (new_state & OUTENABLED)
	enable();
      else
	disable();
    }

  _state = new_state;
}

PUBLIC
int
Jdb_gzip::write(char const *str, size_t len) override
{
  gz_write(str, len);
  return len;
}

PUBLIC static
Console*
Jdb_gzip::console()
{
  static Jdb_gzip c;
  return &c;
}

PUBLIC
Mword
Jdb_gzip::get_attributes() const override
{
  return GZIP | OUT;
}

PUBLIC static FIASCO_INIT
void
Jdb_gzip::init()
{
  Kconsole::console()->register_console(console());
}

STATIC_INITIALIZE(Jdb_gzip);
