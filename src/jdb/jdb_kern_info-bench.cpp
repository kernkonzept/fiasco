INTERFACE:

#include "jdb.h"

class Jdb_kern_info_bench : public Jdb_kern_info_module
{
private:
  static Unsigned64 get_time_now();
  static void show_arch();
};

//---------------------------------------------------------------------------
IMPLEMENTATION:

static Jdb_kern_info_bench k_a INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_bench::Jdb_kern_info_bench()
  : Jdb_kern_info_module('b', "Benchmark privileged instructions")
{
  Jdb_kern_info::register_subcmd(this);
}

PUBLIC
void
Jdb_kern_info_bench::show()
{
  do_mp_benchmark();
  show_arch();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

PRIVATE
void
Jdb_kern_info_bench::do_mp_benchmark()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [mp && (ia32 || amd64)]:

#include "idt.h"

PRIVATE static inline
void
Jdb_kern_info_bench::stop_timer()
{
  Timer_tick::set_vectors_stop();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [mp && !(ia32 || amd64)]:

PRIVATE static inline
void
Jdb_kern_info_bench::stop_timer()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "ipi.h"

static int volatile ipi_bench_spin_done;
static int ipi_cnt;

PRIVATE static
void
Jdb_kern_info_bench::wait_for_ipi(Cpu_number cpu)
{
  Jdb::restore_irqs(cpu);
  stop_timer();
  Proc::sti();

  while (!ipi_bench_spin_done)
    Proc::pause();

  Proc::cli();
  Jdb::save_disable_irqs(cpu);
}

PRIVATE static
void
Jdb_kern_info_bench::empty_func(Cpu_number, void *)
{
  ++ipi_cnt;
}

PRIVATE static
void
Jdb_kern_info_bench::do_ipi_bench(Cpu_number my_cpu, Cpu_number partner)
{
  Unsigned64 time;
  enum {
    Runs2  = 3,
    Warmup = 4,
    Rounds = (1 << Runs2) + Warmup,
  };
  unsigned i;

  ipi_cnt = 0;
  Mem::barrier();

  for (i = 0; i < Warmup; ++i)
    Jdb::remote_work_ipi(my_cpu, partner, empty_func, 0, true);

  time = get_time_now();
  for (i = 0; i < (1 << Runs2); i++)
    Jdb::remote_work_ipi(my_cpu, partner, empty_func, 0, true);

  printf(" %2u:%8llu", cxx::int_value<Cpu_number>(partner),
         (get_time_now() - time) >> Runs2);

  if (ipi_cnt != Rounds)
    printf("\nCounter mismatch: cnt=%d v %d\n", ipi_cnt, Rounds);

  ipi_bench_spin_done = 1;
  Mem::barrier();
}

PRIVATE
void
Jdb_kern_info_bench::do_mp_benchmark()
{
  // IPI bench matrix
  printf("IPI round-trips:\n");
  for (Cpu_number u = Cpu_number::first(); u < Config::max_num_cpus(); ++u)
    if (Cpu::online(u))
      {
        printf("l%2u: ", cxx::int_value<Cpu_number>(u));

	for (Cpu_number v = Cpu_number::first(); v < Config::max_num_cpus(); ++v)
	  if (Cpu::online(v))
	    {
	      if (u == v)
		printf(" %2u:%8s", cxx::int_value<Cpu_number>(u), "X");
	      else
		{
		  ipi_bench_spin_done = 0;

		  // v is waiting for IPIs
		  if (v != Cpu_number::boot_cpu())
		    Jdb::remote_work(v, wait_for_ipi, false);

		  // u is doing benchmark
		  if (u == Cpu_number::boot_cpu())
		    do_ipi_bench(Cpu_number::boot_cpu(), v);
		  else
                    Jdb::remote_work(u, [this, v](Cpu_number cpu){ do_ipi_bench(cpu, v); }, false);

		  // v is waiting for IPIs
		  if (v == Cpu_number::boot_cpu())
		    wait_for_ipi(Cpu_number::boot_cpu());

		  Mem::barrier();

		  while (!ipi_bench_spin_done)
		    Proc::pause();
		}
	    }
	printf("\n");
      }
}
