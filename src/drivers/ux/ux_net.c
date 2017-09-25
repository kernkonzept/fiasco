/*
 * Net helper for Fiasco-UX
 *
 * Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/poll.h>
#include <linux/if_tun.h>
#include <signal.h>

#define PROGNAME "ux_tun"

static inline void generate_irq()
{
  if (write(0, "I", 1) == -1) {
    printf("%s: Communication problems with Fiasco-UX, dying...!\n",
           PROGNAME);
    exit(0);
  }
}

static int pollfds = 1;

static void sig_handler(int sig)
{
  (void)sig;
  pollfds = 1;
}

void loop(int fd)
{
  struct pollfd p;
  int ret;

  p.fd     = fd;
  p.events = POLLIN;

  signal(SIGUSR1, sig_handler);

  while (1) {
    if ((ret = poll(&p, pollfds, 4000)) >= 0) {
      pollfds = 0;
      // also ping for ret==0 so that this program can exit if fiasco goes
      // away, the other side must cope with that
      generate_irq();
    }
  }
}

void usage(char *prog)
{
  printf("%s options\n\n"
	 "  -t fd        TUN file descriptor\n"
         ,prog);
}

int main(int argc, char **argv)
{
  int c;
  int tun_fd = 0;
  struct ifreq ifr;

  while ((c = getopt(argc, argv, "t:")) != -1) {
    switch (c) {
      case 't':
	tun_fd = atol(optarg);
	break;
      default:
	usage(*argv);
	break;
    }
  }

  if (!tun_fd) {
    fprintf(stderr, "%s: Invalid arguments!\n", PROGNAME);
    usage(*argv);
    exit(1);
  }

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  strncpy(ifr.ifr_name, "tun%d", IFNAMSIZ);
  if (ioctl(tun_fd, TUNSETIFF, (unsigned long)&ifr) != 0) {
    perror("ioctl on tun device");
    exit(1);
  }

  fcntl(tun_fd, F_SETFL, O_NONBLOCK);

  loop(tun_fd);

  return 0;
}
