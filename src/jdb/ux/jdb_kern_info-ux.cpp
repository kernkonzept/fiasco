IMPLEMENTATION [ux]:

#include <cstdio>
#include <fcntl.h>

#include "cpu.h"
#include "perf_cnt.h"
#include "simpleio.h"
#include "space.h"
#include "jdb_kobject.h"


class Jdb_kern_info_misc : public Jdb_kern_info_module
{
};

static Jdb_kern_info_misc k_i INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_misc::Jdb_kern_info_misc()
  : Jdb_kern_info_module('i', "Miscellaneous info")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_misc::show()
{
  printf ("clck: %08x.%08x\n",
	  (unsigned) (Kip::k()->clock >> 32), 
	  (unsigned) (Kip::k()->clock));
  show_pdir();
}


class Jdb_kern_info_cpu : public Jdb_kern_info_module
{
};

static Jdb_kern_info_cpu k_c INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_cpu::Jdb_kern_info_cpu()
  : Jdb_kern_info_module('c', "CPU features")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_cpu::show()
{
  const char *perf_type = Perf_cnt::perf_type();
  char cpu_mhz[32];
  char time[32];
  unsigned hz;

  cpu_mhz[0] = '\0';
  if ((hz = Cpu::boot_cpu()->frequency()))
    {
      unsigned mhz = hz / 1000000;
      hz -= mhz * 1000000;
      unsigned khz = hz / 1000;
      snprintf(cpu_mhz, sizeof(cpu_mhz), "%d.%03d MHz", mhz, khz);
    }

  printf ("CPU: %s %s\n", Cpu::boot_cpu()->model_str(), cpu_mhz);
  Cpu::boot_cpu()->show_cache_tlb_info("     ");
  show_features();

  if (Cpu::boot_cpu()->tsc())
    {
      Unsigned32 hour, min, sec, ns;
      Cpu::boot_cpu()->tsc_to_s_and_ns(Cpu::boot_cpu()->rdtsc(), &sec, &ns);
      hour = sec  / 3600;
      sec -= hour * 3600;
      min  = sec  / 60;
      sec -= min  * 60;
      snprintf(time, sizeof(time), "%02d:%02d:%02d.%06d",
	  hour, min, sec, ns/1000);
    }
  else
    strcpy(time, "not available");

  printf("\nPerformance counters: %s"
	 "\nTime stamp counter: %s"
         "\n",
	 perf_type ? perf_type : "no",
	 time);
}

class Jdb_kern_info_host : public Jdb_kern_info_module
{
};

static Jdb_kern_info_host k_H INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_host::Jdb_kern_info_host()
  : Jdb_kern_info_module('H', "Host information")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_host::show()
{
  for (Kobject_dbg::Iterator i = Kobject_dbg::begin(); i != Kobject_dbg::end(); ++i)
    {
      Task const *task = cxx::dyn_cast<Task const *>(Kobject::from_dbg(*i));
      if (!task)
	continue;

      String_buf<64> buf;

      Jdb_kobject::obj_description(&buf, true, *i);
      printf("%s, host-pid=%d\n", buf.c_str(), task->pid());

      buf.reset();
      buf.printf("/proc/%d/maps", task->pid());
      int fd = open(buf.c_str(), O_RDONLY);
      if (fd >= 0)
	{
	  int r;
	  do
	    {
	      r = read(fd, buf.remaining_buffer(), buf.space());
	      if (r > 0)
		printf("%.*s", r, buf.begin());
	    }
	  while (r == buf.space());
	  close(fd);
	}

      printf("\n");
    }
}
