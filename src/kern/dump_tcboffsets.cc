#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "globalconfig.h"

#ifndef CONFIG_BIG_ENDIAN
#define TARGET_BYTE_ORDER(a) a
#else
#define TARGET_BYTE_ORDER(a) len == 4 ? htonl(a) : a
#endif 

#define READ_VAL \
  ({read(fd, &val, len); TARGET_BYTE_ORDER(val);})


#define DUMP_OFFSET(prefix,name,offset) \
  printf("#define OFS__" #prefix "__" #name " 0x%llx\n", READ_VAL);

#define DUMP_BITSHIFT(prefix, value) \
  printf("#define SHIFT__" #prefix " 0x%llx\n", log2(READ_VAL));

#define DUMP_MEMBER1(prefix, type1, member1, name) \
  DUMP_OFFSET(prefix, name, READ_VAL)

#define DUMP_CONSTANT(prefix, value) \
  printf("#define VAL__" #prefix " 0x%llx\n", READ_VAL);

#define DUMP_THREAD_STATE(value) \
  printf("#define VAL__" #value " 0x%llx\n", READ_VAL);

#define DUMP_CAST_OFFSET(type, subtype) \
  printf("#define CAST__" #type "_TO_" #subtype " 0x%llx\n", READ_VAL);

/**
 * Calculates the logarithm base 2 from the given 2^n integer.
 * @param value the 2^n integer
 * @return the log base 2 of value, the exponent n
 */
int log2(int value)
{
  unsigned c = 0; // c will be lg(v)
  while (value >>= 1)
    c++;
  return c;
}

int main(int /*argc*/, char **argv)
{
  /* Only little endian supported if build architecture is 32 bit and 
   * target architecture 64 bit! */
  unsigned long long val = 0;
  int fd = open(argv[1], O_RDONLY); 
  char len = 1;
  len = READ_VAL;
  lseek(fd, 32, SEEK_SET);
#include "tcboffset_in.h"
  close(fd);
  return 0;
}
