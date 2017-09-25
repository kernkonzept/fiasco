#include <stdio.h>
#include <stdlib.h>

#include "zlib.h"
#include "zutil.h"
#include "gzip.h"
#include "panic.h"

static z_stream      gz_stream;
static char          gz_out_buffer[45];
static unsigned long gz_crc = 0;
static int           gz_magic[2] = {0x1f, 0x8b};
static void          (*raw_write)(const char *s, size_t len);

/* ENC is the basic 1 character encoding function to make a char printing */
#define	ENC(c) ((c) ? ((c) & 077) + ' ': '`')

static void
raw_putchar(int c)
{
  const char s[] = { c };
  raw_write(s, 1);
}

static void
uu_open(const char *name)
{
  raw_write("\nbegin 644 ", 11);
  raw_write(name, strlen(name));
  raw_write("\n", 1);
}

static void
uu_close(void)
{
  char ch = ENC('\0');

  raw_putchar(ch);
  raw_write("\nend\n", 5);
}

static void
uu_write(const char *in, int len)
{
  register int ch, n;
  register const char *p;
  static char buf[48];

  while (len > 0)
    {
      if (len >= 45)
	{
	  n = 45;
	}
      else
	{
	  n = len;
	}
      memcpy(buf, in, n);
      memset(buf+n, 0, sizeof(buf)-n);
      len -= n;
      in += n;
      
      ch = ENC(n);
      raw_putchar(ch);
      for (p = buf; n > 0; n -= 3, p += 3)
	{
	  ch = *p >> 2;
	  ch = ENC(ch);
	  raw_putchar(ch);
	  ch = ((*p << 4) & 060) | ((p[1] >> 4) & 017);
	  ch = ENC(ch);
	  raw_putchar(ch);
	  ch = ((p[1] << 2) & 074) | ((p[2] >> 6) & 03);
	  ch = ENC(ch);
	  raw_putchar(ch);
	  ch = p[2] & 077;
	  ch = ENC(ch);
	  raw_putchar(ch);
	}
      raw_putchar('\n');
    }
}

static void *malloc_array[16];
static void *malloc_ptr, *malloc_init_ptr;
static unsigned malloc_size, malloc_init_size;

// linear allocator: allocate memory region
void*
gzip_calloc(unsigned elem, unsigned size)
{
  unsigned i;
  
  size *= elem;
 
  if (size > malloc_size)
    panic("gzip.c: no memory available (need %08x bytes, have %08x bytes\n",
	  size, malloc_size);
  
  for (i=0; i<sizeof(malloc_array)/sizeof(malloc_array[0]); i++)
    {
      if (malloc_array[i] == 0)
	{
	  void *ptr;
	  
	  ptr = malloc_array[i] = malloc_ptr;
	  malloc_ptr  += size;
	  malloc_size -= size;
	  memset(ptr, 0, size);
	  return ptr;
	}
    }
  
  panic("gzip.c: no malloc ptr available\n");
}

// linear allocator: free memory region
void
gzip_free(void *ptr)
{
  unsigned i;

  for (i=0; i<sizeof(malloc_array)/sizeof(malloc_array[0]); i++)
    {
      if (malloc_array[i] == ptr)
	{
	  malloc_array[i] = 0;
	  return;
	}
    }
  
  panic("gzip.c: malloc ptr %p not found\n", ptr);
}

void
gz_init(void *ptr, unsigned size,
	void (*_raw_write)(const char *s, size_t len))
{
  raw_write   = _raw_write;
  malloc_ptr  = malloc_init_ptr  = ptr;
  malloc_size = malloc_init_size = size;
  memset(malloc_ptr, 0, size);
  memset(malloc_array, 0, sizeof(malloc_array));
}

void
gz_end(void)
{
  if (gz_stream.state != NULL)
    deflateEnd(&gz_stream);
}

int
gz_open(const char *fname)
{
  int error;
  
  malloc_ptr  = malloc_init_ptr;
  malloc_size = malloc_init_size;
  
  uu_open(fname);
  
  sprintf(gz_out_buffer, "%c%c%c%c%c%c%c%c%c%c",
          gz_magic[0], gz_magic[1], Z_DEFLATED, 
	  0, /*flags*/
	  0, 0, 0, 0, /* time*/
	  0, /*xflags*/
	  0x03 /*os code=UNIX*/);
  
  gz_stream.zalloc    = (alloc_func)0;
  gz_stream.zfree     = (free_func)0;
  gz_stream.opaque    = (voidpf)0;
  gz_stream.next_in   = Z_NULL;
  gz_stream.next_out  = Z_NULL;
  gz_stream.avail_in  = 0;
  gz_stream.avail_out = 0;
  
  if ((error = deflateInit2(&gz_stream, 
			    Z_BEST_COMPRESSION, Z_DEFLATED,
			    -MAX_WBITS, DEF_MEM_LEVEL,
			    Z_DEFAULT_STRATEGY)) != Z_OK)
    {
      printf("Error \"%s\" in deflateInit()\n", ERR_MSG(error));
      gz_end();
      return -1;
    }
  
  gz_stream.next_out  = (Bytef*)(gz_out_buffer + 10);
  gz_stream.avail_out = sizeof(gz_out_buffer) - 10;
  gz_stream.avail_in  = 0;
  gz_crc              = crc32(0L, Z_NULL, 0);

  return 0;
}

static void
gz_flush_outbuffer(void)
{
  int len = sizeof(gz_out_buffer) - gz_stream.avail_out;
  
  if (len > 0)
    {
      uu_write(gz_out_buffer, len);
      gz_stream.next_out  = (Bytef*)gz_out_buffer;
      gz_stream.avail_out = sizeof(gz_out_buffer);
    }
}

int
gz_write(const char *data, unsigned int size)
{
  gz_stream.next_in  = (Bytef*)data;
  gz_stream.avail_in = size;

  while (gz_stream.avail_in != 0)
    {
      int error;
      
      if (gz_stream.avail_out == 0)
	gz_flush_outbuffer();
      
      if ((error = deflate(&gz_stream, Z_NO_FLUSH)) != Z_OK)
	{
	  printf("Error \"%s\" in deflate()\n", ERR_MSG(error));
	  gz_end();
	  return -1;
	}
    }

  gz_crc = crc32(gz_crc, (const Bytef*)data, size);

  return 0;
}

int
gz_close(void)
{
  char buf[8], *bptr = buf;
  int  error;
  unsigned len;
  
  do
    {
      if (gz_stream.avail_out == 0)
	gz_flush_outbuffer();
      
      error = deflate(&gz_stream, Z_FINISH);
    } while (error == Z_OK);
  
  if (error != Z_STREAM_END)
    {
      printf("Error \"%s\" deflate() (flush)\n", ERR_MSG(error));
      gz_end();
      return -1;
    }

  if (gz_stream.avail_out == 0)
    gz_flush_outbuffer();
  
  ((unsigned*)bptr)[0] = gz_crc;
  ((unsigned*)bptr)[1] = gz_stream.total_in;
  len = 8;

  if (gz_stream.avail_out < len)
    {
      unsigned tmp_len = gz_stream.avail_out;
      
      memcpy(gz_stream.next_out, bptr, tmp_len);
      len  -= tmp_len;
      bptr += tmp_len;
      gz_stream.avail_out = 0;
      gz_flush_outbuffer();
    }
  
  memcpy(gz_stream.next_out, bptr, len);
  gz_stream.avail_out -= len;
  gz_flush_outbuffer();
  
  uu_close();
  gz_end();

  return 0;
}

