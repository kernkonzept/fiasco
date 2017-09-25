#ifndef __VPRINTF_BACKEND_H__
#define __VPRINTF_BACKEND_H__

#include <stddef.h>
#include <cdefs.h>

typedef int (output_func)(char const *, size_t, void*);

struct output_op {
  void *data;
  output_func *put;
};

__BEGIN_DECLS

int __v_printf(struct output_op* fn, const char *format, va_list arg_ptr);

__END_DECLS


#endif // __VPRINTF_BACKEND_H__
