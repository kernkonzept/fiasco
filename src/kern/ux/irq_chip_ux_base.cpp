INTERFACE:

#include "config.h"
#include <sys/poll.h>

class Irq_chip_ux_base
{
public:
  enum
  {
    Irq_timer    = 0,
    Irq_con      = 1,
    Irq_net      = 2,
    Num_dev_irqs = 4,
    Irq_ipi_base = 4,
    Num_irqs     = 4 + Config::Max_num_cpus,
  };

  unsigned int pid_for_irq_prov(unsigned irq)
  {
    return pids[irq];
  }

  static Irq_chip_ux_base *main;

protected:
  unsigned _base_vect = 0x20;
  unsigned int highest_irq;
  unsigned int pids[Num_irqs];
  pollfd pfd[Num_irqs];
};

IMPLEMENTATION:

#include <cassert>
#include <csignal>
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <fcntl.h>
#include <pty.h>
#include <sys/types.h>

#include "emulation.h"

Irq_chip_ux_base *Irq_chip_ux_base::main;

PUBLIC
void
Irq_chip_ux_base::shutdown()
{
  for (auto pid: pids)
    if (pid)
      kill(pid, SIGTERM);
}

PRIVATE
void
Irq_chip_ux_base::_set_owner(int pid)
{
  for (pollfd *p = pfd; p != pfd + highest_irq; ++p)
    if (p->fd && (p->events & POLLIN))
      fcntl(p->fd, F_SETOWN, pid);
}

PUBLIC static inline
void
Irq_chip_ux_base::set_owner(int pid)
{
  main->_set_owner(pid);
}

PUBLIC
int
Irq_chip_ux_base::_pending_vector()
{
  for (unsigned i = 0; i < highest_irq; ++i)
    pfd[i].revents = 0;

  if (poll(pfd, highest_irq, 0) <= 0)
    return -1;

  for (unsigned i = 0; i < highest_irq; ++i)
    if (pfd[i].revents & POLLIN)
      {
        // clear all queued IRQs
        char buffer[8];
        while (read(pfd[i].fd, buffer, sizeof (buffer)) > 0)
          ;

        if (Emulation::idt_vector_present(i + _base_vect))
          return i + _base_vect;
      }

  return -1;
}

PUBLIC static inline
int
Irq_chip_ux_base::pending_vector()
{
  return main->_pending_vector();
}

static bool prepare_irq(int sockets[2])
{
  struct termios tt;

  if (openpty(&sockets[0], &sockets[1], NULL, NULL, NULL))
    {
      perror("openpty");
      return false;
    }

  if (tcgetattr(sockets[0], &tt) < 0)
    {
      perror("tcgetattr");
      return false;
    }

  cfmakeraw(&tt);

  if (tcsetattr(sockets[0], TCSADRAIN, &tt) < 0)
    {
      perror("tcsetattr");
      return false;
    }

  fflush(NULL);

  return true;
}

PUBLIC
bool
Irq_chip_ux_base::setup_irq_prov(unsigned irq, const char *const path,
                                 void (*bootstrap_func)())
{
  assert (irq < Num_irqs);
  int sockets[2];

  if (access(path, X_OK | F_OK))
    {
      perror(path);
      return false;
    }

  if (prepare_irq(sockets) == false)
    return false;

  switch (pids[irq] = fork())
    {
      case -1:
        return false;

      case 0:
        break;

      default:
        close(sockets[1]);
        fcntl(sockets[0], F_SETFD, FD_CLOEXEC);
        pfd[irq].fd = sockets[0];
        return true;
    }

  // Unblock all signals except SIGINT, we enter jdb with it and don't want
  // the irq providers to die
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigprocmask(SIG_SETMASK, &mask, NULL);

  fclose(stdin);
  fclose(stdout);
  fclose(stderr);

  dup2(sockets[1], 0);
  close(sockets[0]);
  close(sockets[1]);
  bootstrap_func();

  _exit(EXIT_FAILURE);
}

