
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

static char buffer[128];
static int  buf_idx;
static int  buf_chars;

static char const *hexchars = "0123456789abcdef";

extern "C" void exit(int status);

int
fill_buffer(int fd)
{
  buf_chars = read(fd, buffer, sizeof(buffer));
  buf_idx   = 0;

  return buf_chars >= 1;
}

int
this_char(int fd, char *c)
{
  if (buf_idx >= buf_chars)
    {
      if (!fill_buffer(fd))
	return 0;
    }

  *c = buffer[buf_idx];
  return 1;
}

int
skip_line(int fd)
{
  for (;;)
    {
      char c;
      
      if (! this_char(fd, &c))
	return 0;

      buf_idx++;
      
      if (c == '\n')
	return 1;
    }
}

int
skip_to_hex(int fd)
{
  for (;;)
    {
      char c;
  
      if (! this_char(fd, &c))
	return 0;

      if (strchr(hexchars, c))
	return 1;

      if (! skip_line(fd))
	return 0;
    }
}

int
skip_preamble(int fd)
{
  for (;;)
    {
      char c;

      if (! this_char(fd, &c))
	return 0;

      if (!strchr(hexchars, c))
	{
	  if (!skip_line(fd))
	    return 0;

	  continue;
	}

      return 1;
    }
}

int
read_byte(int fd, unsigned *byte)
{
  unsigned hex = 0;
  int i;
  
  for (i=0; i<2; i++)
    {
      char c;
      const char *idx;
      
      if (! this_char(fd, &c))
	return 0;

      if (!(idx = strchr(hexchars, c)))
	exit(-1);
      
      hex *= 16;
      hex += (idx-hexchars);
      
      buf_idx++;
    }

  *byte = hex;
  return 1;
}

int
read_dword(int fd, unsigned *dword)
{
  char c;
  unsigned hex = 0;
  unsigned factor = 1;
  int i;
  
  if (! skip_to_hex(fd))
    return 0;
  
  for (i=0, factor=1; i<4; i++, factor*=256)
    {
      unsigned byte;
      
      if (! read_byte(fd, &byte))
	return 0;
      
      hex += byte * factor;
    }

  this_char(fd, &c);
  buf_idx++; // skip/\n space after dword

  *dword = hex;
  return 1;
}

int
main(int argc, char **argv)
{
  static char buf[20];
  int ret;
  int fd;
  unsigned dword;
  unsigned checksum;

  skip_preamble(STDIN_FILENO);

  checksum = 0;
  while (read_dword(STDIN_FILENO, &dword))
    checksum += dword;

  printf("%08x", checksum);

  return 0;
}

