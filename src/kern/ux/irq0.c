
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

int main (void) {

  sigset_t blocked;
  struct itimerval t;
  int sig;

  sigemptyset (&blocked);
  sigaddset   (&blocked, SIGALRM);
  sigprocmask (SIG_BLOCK, &blocked, NULL);

  t.it_interval.tv_sec  = t.it_value.tv_sec  = 0;
  t.it_interval.tv_usec = t.it_value.tv_usec = 10000;
  setitimer (ITIMER_REAL, &t, NULL);

  for (;;)
    {  
      switch (sigwait (&blocked, &sig))
        {
          case 0:
            if (write (0, "T", 1) != -1)
              continue;

          case -1:
            if (errno != EINTR)
              return 1;
        }
    }

  return 0;
}
