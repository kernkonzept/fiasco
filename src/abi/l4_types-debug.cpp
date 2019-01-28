IMPLEMENTATION [debug]:

#include <cstdio>
#include "simpleio.h"

PUBLIC void L4_timeout::print() const
{
  printf("m=%lu e=%lu", man(), exp());
}

PUBLIC void L4_timeout_pair::print() const
{
  printf("snd: ");
  snd.print();
  printf(" rcv: ");
  rcv.print();
}

PUBLIC
void
Utcb::print(char const *clreol_lf = "\n") const
{
  printf("Values:%s", clreol_lf);
  for (unsigned i = 0; i < Max_words; ++i)
    printf("%2u:%16lx%s", i, values[i], !((i+1) % 4) ? clreol_lf : " ");

  if (Max_words % 4)
    putstr(clreol_lf);

  printf("Reserved:    %16lx%s", utcb_addr, clreol_lf);

  printf("Buffers: dsc=%16lx%s", buf_desc.raw(), clreol_lf);
  for (unsigned i = 0; i < sizeof(buffers) / sizeof(buffers[0]); ++i)
    printf("%2u:%16lx%s", i, buffers[i], !((i+1) % 4) ? clreol_lf : " ");

  if ((sizeof(buffers) / sizeof(buffers[0])) % 4)
    putstr(clreol_lf);

  printf("Xfer timeout: ");
  xfer.print();
  putstr(clreol_lf);

  printf("Error:       %16lx%s", error.raw(), clreol_lf);
  printf("User values: %16lx %16lx %16lx%s", user[0], user[1], user[2], clreol_lf);
}
