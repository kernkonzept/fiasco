/*
 * Simple 'raise' implementation required by some gcc versions.
 *
 * Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 */

#include <assert.h>

int raise(int x);

int raise(int x)
{
  (void)x;
  assert(0);
  return 0;
}
