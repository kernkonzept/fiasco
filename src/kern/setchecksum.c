#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <inttypes.h>


static void usage(void)
{
  printf("Usage: set_checksum <kernel_image_name> <ro_checksum> <rw_checksum>\n");
}

static int fd;
static void *image;
static struct stat stats;
static char const * const match = "FIASCOCHECKSUM=";


int main(int argc, char *argv[])
{
  void *base;
  uint32_t volatile *chksum;
  unsigned long left;
  struct timespec ts[2];

  if (argc!=4)
    {
      usage();
      exit(1);
    }

  fd = open(argv[1], O_RDWR | O_EXCL);
  if (fd < 0)
    {
      fprintf(stderr, "error opening kernel image: '%s': ", argv[1]);
      perror("");
      exit(1);
    }

  if (fstat(fd, &stats) < 0)
    {
      perror("error: can't stat kernel image");
      exit(1);
    }

  image = mmap(NULL, stats.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (image == MAP_FAILED)
    {
      perror("error: can't mmap kernel image");
      exit(1);
    }

  base = image;
  left = stats.st_size;
  while ((base = memchr(base, match[0], left)))
    {
      if (memcmp(match, base, strlen(match) + 1) == 0)
        break;

      base = (char*)base + 1;
      left = stats.st_size - (base - image);
    }

  if (!base)
    {
      fprintf(stderr,"error: magic string '%s' not found in kernel image\n", match);
      return 2;
    }

  base = (char*)base + strlen(match) + 1;

  chksum = base;
  *chksum = strtoul(argv[2], NULL, 0);
  *(chksum + 1) = strtoul(argv[3], NULL, 0);

  if (munmap(image, stats.st_size) < 0)
    {
      perror("error: can't unmap kernel image");
      exit(1);
    }

  // restore original access and modification time
  ts[0].tv_sec  = stats.st_atim.tv_sec;
  ts[0].tv_nsec = stats.st_atim.tv_nsec;
  ts[1].tv_sec  = stats.st_mtim.tv_sec;
  ts[1].tv_nsec = stats.st_mtim.tv_nsec;
  if (futimens(fd, ts) < 0)
    perror("error: resetting modification time");

  if (close(fd) < 0)
    {
      fprintf(stderr, "error closing kernel image: '%s': ", argv[1]);
      perror("");
      exit(2);
    }

  return 0;
}
