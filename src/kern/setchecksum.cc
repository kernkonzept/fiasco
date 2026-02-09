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

static char const *const match = "FIASCOCHECKSUM=";

int main(int argc, char *argv[])
{
  if (argc != 4)
    {
      usage();
      exit(1);
    }

  int fd = open(argv[1], O_RDWR | O_EXCL);
  if (fd < 0)
    {
      fprintf(stderr, "error opening kernel image: '%s': ", argv[1]);
      perror("");
      exit(1);
    }

  struct stat stats;
  if (fstat(fd, &stats) < 0)
    {
      perror("error: can't stat kernel image");
      exit(1);
    }

  auto *image = static_cast<char *>(mmap(NULL, stats.st_size,
                                         PROT_READ | PROT_WRITE,
                                         MAP_SHARED, fd, 0));
  if (image == MAP_FAILED)
    {
      perror("error: can't mmap kernel image");
      exit(1);
    }

  char *base = image;
  unsigned long left = stats.st_size;
  while ((base = static_cast<char *>(memchr(base, match[0], left))))
    {
      if (memcmp(match, base, strlen(match) + 1) == 0)
        break;

      base = base + 1;
      left = stats.st_size - (base - image);
    }

  if (!base)
    {
      fprintf(stderr,"error: magic string '%s' not found in kernel image\n", match);
      return 2;
    }

  base = base + strlen(match) + 1;

  auto *chksum = reinterpret_cast<uint32_t volatile *>(base);
  *chksum = strtoul(argv[2], NULL, 0);
  *(chksum + 1) = strtoul(argv[3], NULL, 0);

  if (munmap(image, stats.st_size) < 0)
    {
      perror("error: can't unmap kernel image");
      exit(1);
    }

  // restore original access and modification time
  struct timespec ts[2];
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
