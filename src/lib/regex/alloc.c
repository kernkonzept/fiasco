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
#include <types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct {
  void*  next;
  size_t size;
} __alloc_t;

#define BLOCK_START(b)	(((void*)(b))-sizeof(__alloc_t))
#define BLOCK_RET(b)	(((void*)(b))+sizeof(__alloc_t))

#define MEM_BLOCK_SIZE	PAGE_SIZE
#define PAGE_SIZE	4096U
#define PAGE_ALIGN(s)	(((s)+PAGE_SIZE-1) & (~(PAGE_SIZE-1)))

static __alloc_t* __small_mem[8];

#define __SMALL_NR(i)		(MEM_BLOCK_SIZE/(i))

#define __MIN_SMALL_SIZE	__SMALL_NR(256)		/*   16 /   32 */
#define __MAX_SMALL_SIZE	__SMALL_NR(2)		/* 2048 / 4096 */

#define GET_SIZE(s)		(__MIN_SMALL_SIZE<<get_index((s)))
#define FIRST_SMALL(p)		(((unsigned long)(p))&(~(MEM_BLOCK_SIZE-1)))


static char*    linear_mem;
static unsigned linear_mem_pages;
static unsigned linear_mem_free;

static void*
regex_mmap(size_t size)
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
regex_munmap(void *ptr, size_t size)
{
  Address i, j;

  size = PAGE_ALIGN(size);
  i = ((Address)ptr - (Address)linear_mem) / PAGE_SIZE;
  j = (1<<(size/PAGE_SIZE))-1;
  linear_mem_free |= j<<i;
}

static void*
regex_mremap(void *ptr, size_t old_size, size_t new_size)
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
  if ((new_ptr = regex_mmap(new_size)) == MAP_FAILED)
    return MAP_FAILED;

  memcpy(new_ptr, ptr, old_size);
  regex_munmap(ptr, old_size);
  return new_ptr;
}

void
regex_reset(void)
{
  linear_mem_free = (1<<linear_mem_pages)-1;
  memset(__small_mem, 0, sizeof(__small_mem));
}

void
regex_init(void *ptr, unsigned size)
{
  if (size > 31*PAGE_SIZE)
    size = 31*PAGE_SIZE;
  linear_mem_pages = size / PAGE_SIZE;
  linear_mem       = ptr;
  regex_reset();
}

static inline int
__ind_shift()
{
  return (MEM_BLOCK_SIZE==4096) ? 4 : 5;
}

static size_t
get_index(size_t _size)
{
  register size_t idx=0;
  if (_size)
    {
      register size_t size=((_size-1)&(MEM_BLOCK_SIZE-1))>>__ind_shift();
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
      register int i,nr;
      ptr=regex_mmap(MEM_BLOCK_SIZE);
      if (ptr==MAP_FAILED)
	return MAP_FAILED;

      __small_mem[idx]=ptr;

      nr=__SMALL_NR(size)-1;
      for (i=0;i<nr;i++)
	{
	  ptr->next=(((void*)ptr)+size);
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
regex_free(void *ptr)
{
  register size_t size;
  if (ptr) 
    {
      size=((__alloc_t*)BLOCK_START(ptr))->size;
      if (size) 
	{
	  if (size<=__MAX_SMALL_SIZE)
	    __small_free(ptr,size);
	  else
	    regex_munmap(BLOCK_START(ptr),size);
	}
    }
}

void*
regex_malloc(size_t size)
{
  __alloc_t* ptr;
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
	ptr=regex_mmap(need);
    }
  if (ptr==MAP_FAILED)
    return 0;
  ptr->size=need;
  return BLOCK_RET(ptr);
}

void*
regex_realloc(void* ptr, size_t _size)
{
  register size_t size=_size;
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
		  void *new=regex_malloc(_size);
		  if (new)
		    {
		      register __alloc_t* foo=BLOCK_START(new);
		      size=foo->size;
		      if (size>tmp->size)
			size=tmp->size;
		      if (size)
	      		memcpy(new,ptr,size-sizeof(__alloc_t));
		      regex_free(ptr);
		    }
		  return new;
		}
	      else
		{
		  register __alloc_t* foo;
		  size=PAGE_ALIGN(size);
		  foo=regex_mremap(tmp,tmp->size,size);
		  if (foo==MAP_FAILED)
		    return 0;
		  else
		    {
		      foo->size=size;
		      return BLOCK_RET(foo);
		    }
		}
	    }
	}
      else
	{ /* size==0 */
	  regex_free(ptr);
	  return 0;
	}
    }
  else
    { /* ptr==0 */
      if (size)
	return regex_malloc(size);
    }
  return ptr;
}

