/* from Dietlibc */

/*
 * malloc/free by O.Dreesen
 *
 * first TRY:
 *   lists w/magics
 * and now the second TRY
 *   let the kernel map all the stuff (if there is something to do)
 */

#include <cdefs.h>
#include <panic.h>
#include <types.h>
#include <string.h>
#include <simple_malloc.h>

#include "kmem_alloc.h"
#include "static_init.h"

#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct __alloc_t {
  struct __alloc_t *next;
  size_t size;
} __alloc_t;

#define BLOCK_START(b)	(((__alloc_t*)(b))-1)
#define BLOCK_RET(b)	(((char*)(b))+sizeof(__alloc_t))

#define MEM_BLOCK_SIZE	PAGE_SIZE
#define PAGE_SIZE	4096U
#define PAGE_ALIGN(s)	(((s)+PAGE_SIZE-1) & (~(PAGE_SIZE-1)))

static __alloc_t* __small_mem[8];

#define __SMALL_NR(i)		(MEM_BLOCK_SIZE/(i))

#define __MIN_SMALL_SIZE	__SMALL_NR(256)		/*   16 /   32 */
#define __MAX_SMALL_SIZE	__SMALL_NR(2)		/* 2048 / 4096 */

#define GET_SIZE(s)		(__MIN_SMALL_SIZE<<get_index((s)))


static char*    linear_mem;
static unsigned linear_mem_pages;
static unsigned linear_mem_free;

static void*
simple_mmap(size_t size)
{
  Address j;
  char *ptr;

  size = PAGE_ALIGN(size);
  j    = (1<<(size/PAGE_SIZE))-1;
  ptr  = linear_mem;
  for (;;)
    {
      if ((linear_mem_free & j) == j)
	{
	  linear_mem_free &= ~j;
	  return ptr;
	}
      if (j & 0x80000000)
	break;
      ptr += PAGE_SIZE;
      j<<=1;
    }
  return MAP_FAILED;
}

static void
simple_munmap(void *ptr, size_t size)
{
  Address i, j;

  size = PAGE_ALIGN(size);
  i = ((Address)ptr - (Address)linear_mem) / PAGE_SIZE;
  j = (1<<(size/PAGE_SIZE))-1;
  linear_mem_free |= j<<i;
}

static void*
simple_mremap(void *ptr, size_t old_size, size_t new_size)
{
  Address i, j, k;
  void *new_ptr;

  old_size = PAGE_ALIGN(old_size);
  new_size = PAGE_ALIGN(new_size);
  i = ((Address)ptr - (Address)linear_mem) / PAGE_SIZE;
  j = (1<<(old_size/PAGE_SIZE))-1;
  k = (1<<(new_size/PAGE_SIZE))-1;

  if (linear_mem_free & (j<<i))
    return MAP_FAILED;
  if (k <= j)
    {
      linear_mem_free |= (k ^ j) << i;
      return ptr;
    }
  k = (k ^ j) << i;
  if ((linear_mem_free & k) == k)
    {
      linear_mem_free &= ~k;
      return ptr;
    }
  if ((new_ptr = simple_mmap(new_size)) == MAP_FAILED)
    return MAP_FAILED;

  memcpy(new_ptr, ptr, old_size);
  simple_munmap(ptr, old_size);
  return new_ptr;
}

static void
simple_malloc_reset(void)
{
  linear_mem_free = (1<<linear_mem_pages)-1;
  memset(__small_mem, 0, sizeof(__small_mem));
}

static void
simple_malloc_init(void)
{
  const size_t size = 16*4*1024; // must be less than 32 pages!
  char *heap = (char*)Kmem_alloc::allocator()->alloc(Bytes(size));
  if (!heap)
    panic("No memory for simple_malloc heap");
  linear_mem_pages = size / PAGE_SIZE;
  linear_mem       = heap;
  simple_malloc_reset();
}

static inline int
__ind_shift(void)
{
  return (MEM_BLOCK_SIZE==4096) ? 4 : 5;
}

static size_t
get_index(size_t _size)
{
  size_t idx=0;
  if (_size)
    {
      size_t size=((_size-1)&(MEM_BLOCK_SIZE-1))>>__ind_shift();
      while(size)
	{
	  size>>=1;
	  ++idx;
	}
    }
  return idx;
}

static void
__small_free(void*_ptr,size_t _size)
{
  __alloc_t* ptr=BLOCK_START(_ptr);
  size_t size=_size;
  size_t idx=get_index(size);

  memset(ptr,0,size);	/* allways zero out small mem */

  ptr->next=__small_mem[idx];
  __small_mem[idx]=ptr;
}

static void*
__small_malloc(size_t _size)
{
  __alloc_t *ptr;
  size_t size=_size;
  size_t idx;

  idx=get_index(size);
  ptr=__small_mem[idx];

  if (ptr==0)
    {
      int i,nr;
      ptr=(__alloc_t*)simple_mmap(MEM_BLOCK_SIZE);
      if (ptr==MAP_FAILED)
	return MAP_FAILED;

      __small_mem[idx]=ptr;

      nr=__SMALL_NR(size)-1;
      for (i=0;i<nr;i++)
	{
	  ptr->next=(__alloc_t*)(((char*)ptr)+size);
	  ptr=ptr->next;
	}
      ptr->next=0;

      ptr=__small_mem[idx];
    }

  /* get a free block */
  __small_mem[idx]=ptr->next;
  ptr->next=0;

  return ptr;
}

void
simple_free(void *ptr)
{
  size_t size;
  if (ptr)
    {
      size=(BLOCK_START(ptr))->size;
      if (size)
	{
	  if (size<=__MAX_SMALL_SIZE)
	    __small_free(ptr,size);
	  else
	    simple_munmap(BLOCK_START(ptr),size);
	}
    }
}

void*
simple_malloc(size_t size)
{
  void *ptr;
  size_t need;
  if (!size)
    return 0;
  size+=sizeof(__alloc_t);
  if (size<sizeof(__alloc_t))
    return 0;
  if (size<=__MAX_SMALL_SIZE)
    {
      need=GET_SIZE(size);
      ptr=__small_malloc(need);
    }
  else
    {
      need=PAGE_ALIGN(size);
      if (!need)
	ptr=MAP_FAILED;
      else
	ptr=simple_mmap(need);
    }
  if (ptr==MAP_FAILED)
    return 0;
  ((__alloc_t*)ptr)->size=need;

  // play safe: this is required by cs_mem_malloc()!
  memset(BLOCK_RET(ptr), 0, size);
  return BLOCK_RET(ptr);
}

void*
simple_calloc(size_t nmemb, size_t size)
{
  return simple_malloc(nmemb * size);
}

void*
simple_realloc(void* ptr, size_t _size)
{
  size_t size=_size;
  if (ptr)
    {
      if (size)
	{
	  __alloc_t *tmp=BLOCK_START(ptr);
	  size+=sizeof(__alloc_t);
	  if (size<sizeof(__alloc_t))
	    return 0;
	  size=(size<=__MAX_SMALL_SIZE)?GET_SIZE(size):PAGE_ALIGN(size);
	  if (tmp->size!=size)
	    {
	      if ((tmp->size<=__MAX_SMALL_SIZE))
		{
		  void *new_ptr=simple_malloc(_size);
		  if (new_ptr)
		    {
		      __alloc_t* foo=BLOCK_START(new_ptr);
		      size=foo->size;
		      if (size>tmp->size)
			size=tmp->size;
		      if (size)
			memcpy(new_ptr,ptr,size-sizeof(__alloc_t));
		      simple_free(ptr);
		    }
		  return new_ptr;
		}
	      else
		{
		  void* foo;
		  size=PAGE_ALIGN(size);
		  foo=simple_mremap(tmp,tmp->size,size);
		  if (foo==MAP_FAILED)
		    return 0;
		  else
		    {
		      ((__alloc_t*)foo)->size=size;
		      return BLOCK_RET(foo);
		    }
		}
	    }
	}
      else
	{ /* size==0 */
	  simple_free(ptr);
	  return 0;
	}
    }
  else
    { /* ptr==0 */
      if (size)
	return simple_malloc(size);
    }
  return ptr;
}

STATIC_INITIALIZER_P(simple_malloc_init, JDB_MODULE_INIT_PRIO);
