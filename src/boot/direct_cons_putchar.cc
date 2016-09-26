
#include <string.h>
#include "boot_direct_cons.h"
#include "boot_paging.h"

void
direct_cons_putchar(unsigned char c)
{
  static int ofs = -1;
  unsigned char *vidbase = ((unsigned char*)0xb8000);

  if (ofs < 0)
    {
      ofs = 0;
      direct_cons_putchar('\n');
    }

  switch (c)
    {
    case '\n':
      memcpy(vidbase, vidbase+80*2, 80*2*24);
      memset(vidbase+80*2*24,0, 80*2);
      // FALLTHRU
    case '\r':
      ofs = 0;
      break;

    case '\t':
      ofs = (ofs + 8) & ~7;
      break;

    default:
      if (ofs >= 80)
	direct_cons_putchar('\n');

	{
	  volatile unsigned char *p = vidbase + 80*2*24 + ofs*2;
	  p[0] = c;
	  p[1] = 0x0f;
	  ofs++;
	}
      break;
    }
}
