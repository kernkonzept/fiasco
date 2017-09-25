#include <stdlib.h>

typedef struct
{
  void (*func)(void*, int);
  void *arg;
} atexit_t;

#define NUM_ATEXIT	80

static atexit_t __atexitlist[NUM_ATEXIT];
static int atexit_counter;

int
atexit(void (*func)(void))
{
  if (atexit_counter >= NUM_ATEXIT)
    return -1;

  __atexitlist[atexit_counter].func = (void(*)(void*, int))func;
  __atexitlist[atexit_counter].arg  = 0;
  atexit_counter++;
  return 0;
}

int
__cxa_atexit(void (*func)(void*), void *arg, void *dso_handle)
{
  if (atexit_counter>=NUM_ATEXIT)
    return -1;

  __atexitlist[atexit_counter].func = (void(*)(void*, int))func;
  __atexitlist[atexit_counter].arg  = arg;
  (void)dso_handle;
  atexit_counter++;
  return 0;
}

int
__aeabi_atexit (void *arg, void (*func)(void*), void *dso_handle)
{
  return __cxa_atexit(func, arg, dso_handle);
}

void exit(int code)
{
  while (atexit_counter)
    {
      atexit_t *a = __atexitlist + (--atexit_counter);
      if (a->func)
	a->func(a->arg, code);
    }
  _exit(code);
}
