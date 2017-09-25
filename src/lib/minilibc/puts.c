#include <stdio.h>
#include <string.h>
#include "libc_backend.h"

int puts( const char *c ) 
{  
  unsigned long state = __libc_backend_printf_lock();
  __libc_backend_outs(c,strlen(c)); 
  int ret = __libc_backend_outs("\n",1); 
  __libc_backend_printf_unlock(state);

  return ret;
}

int putstr( const char *const c )
{
  unsigned long state = __libc_backend_printf_lock();
  int ret = __libc_backend_outs(c,strlen(c)); 
  __libc_backend_printf_unlock(state);

  return ret;
}

int putnstr( const char *const c, int len )
{
  unsigned long state = __libc_backend_printf_lock();
  int ret = __libc_backend_outs(c,len); 
  __libc_backend_printf_unlock(state);

  return ret;
}
