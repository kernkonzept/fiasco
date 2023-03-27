int __cxa_atexit(void (*func)(void*), void *arg, void *dso_handle);
int __aeabi_atexit (void *arg, void (*func)(void*), void *dso_handle);

int
__aeabi_atexit (void *arg, void (*func)(void*), void *dso_handle)
{
  return __cxa_atexit(func, arg, dso_handle);
}
