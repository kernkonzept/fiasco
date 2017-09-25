// Simplistic Unix interface for use by lib/gmon.cpp.  We support file
// I/O (translated into uuencoded output), sbrk()-like memory
// allocation, and profile() (setting parameters for profiling).

INTERFACE:

#include "types.h"
#include <cstddef>

extern char *pr_base;
extern Address pr_off;
extern size_t pr_size;
extern size_t pr_scale;

IMPLEMENTATION:

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <panic.h>

#include "kmem_alloc.h"
#include "config.h"
#include "kernel_console.h"

int 
creat(const char* fname, int)
{
  printf("starting to output: %s.uu -- hit Return\n", fname);
//  Kconsole::console()->getchar();
  Kconsole::console()->start_exclusive(Console::GZIP);

  return 6; 
}

void 
perror(const char *s)
{
  panic (s);
}

/*ssize_t*/ size_t
write(int, const char *buf, size_t size)
{
  for (size_t i=0; i<size; i++)
    putchar(*buf++);
  return size;
}

int 
close(int fd)
{
  assert(fd == 6);
  (void)fd;

  Kconsole::console()->end_exclusive(Console::GZIP);

  return 0;
}
  
void *
sbrk(size_t size)
{
  void *ret = Kmem_alloc::allocator()
    ->unaligned_alloc((size+Config::PAGE_SIZE-1) & ~(Config::PAGE_SIZE-1));
  if (ret == 0) 
    ret = (void*)-1;
  else
    memset(ret, 0, size);

  return ret;
}

void 
sbrk_free(void* buf, size_t size)
{
  Kmem_alloc::allocator()
    ->unaligned_free((size+Config::PAGE_SIZE-1)/Config::PAGE_SIZE, buf);
}

char *pr_base;
Address pr_off;
size_t pr_size;
size_t pr_scale;

int
profil(char *samples, size_t size, Address offset, size_t scale)
{
  pr_base  = samples;
  pr_size  = size;
  pr_off   = offset;
  pr_scale = scale;

  return 0;
}
