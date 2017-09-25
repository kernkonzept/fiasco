#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "vprintf_backend.h"

struct str_data {
  char* str;
  size_t len;
  size_t size;
};

static int swrite(void*ptr, size_t nmemb, struct str_data* sd) {
  size_t tmp=sd->size-sd->len;
  if (tmp>0) {
    size_t len=nmemb;
    if (len>tmp) len=tmp;
    memcpy(&sd->str[sd->len],ptr,len);
    sd->len+=len;
    sd->str[sd->len]=0;
  }
  return nmemb;
}

int vsnprintf(char* str, size_t size, const char *format, va_list arg_ptr) {
  struct str_data sd = { str, 0, size };
  struct output_op ap = { &sd, (output_func*)&swrite };
  if (size) --sd.size;
  return __v_printf(&ap,format,arg_ptr);
}
