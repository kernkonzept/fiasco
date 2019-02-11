#pragma once

#include <cdefs.h>

__BEGIN_DECLS

void *simple_malloc(size_t size);
void *simple_calloc(size_t nmemb, size_t size);
void *simple_realloc(void* ptr, size_t _size);
void  simple_free(void *ptr);

__END_DECLS
