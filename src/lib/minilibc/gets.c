#include <stdio.h>
#include <libc_backend.h>


char *gets(char *s)
{
  if(s!=NULL) {
    size_t num;
    num = __libc_backend_ins(s,(size_t)-1);
    s[num-1]=0;
  }
  return s;
}
