IMPLEMENTATION [mp]:

#include <cstdio>
#include "simpleio.h"

#include "jdb.h"
#include "jdb_module.h"
#include "static_init.h"
#include "types.h"
#include "mp_request.h"


class Jdb_mp_request_module : public Jdb_module
{
  typedef Mp_request_queue::Fifo Fifo;
  typedef Fifo::Item Item;
public:
  Jdb_mp_request_module() FIASCO_INIT;
  struct Find_cpu
  {
    Item const *r;
    mutable Cpu_number cpu;
    Find_cpu(Item const *i) : r(i), cpu(~0U) {}
    void operator()(Cpu_number _cpu) const
    {
      if (&Mp_request_queue::rq.cpu(_cpu) == r)
        {
	  cpu = _cpu;
	  return;
	}
    }
  };
};

static Jdb_mp_request_module jdb_mp_request_module INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

PRIVATE static
Cpu_number
Jdb_mp_request_module::find_cpu(Item const *r)
{
  if (!r)
    return Cpu_number::boot_cpu();

  Find_cpu _find_cpu(r);
  Jdb::foreach_cpu(_find_cpu);
  return _find_cpu.cpu;
}

PRIVATE static
void
Jdb_mp_request_module::print_request(Item const *item)
{
  printf("  Request item of cpu %u [%p]:\n"
         "    value  = { func = %p, arg = %p, _lock=%lu }\n"
         "    next   = %p (cpu %u)\n",
         find_cpu(item), item, item->value.func, item->value.arg, item->value._lock,
         item->next, Cpu_number::val(find_cpu(item->next)));
}

PRIVATE static
void
Jdb_mp_request_module::print_queue(Cpu_number cpu)
{

  if (!Jdb::cpu_in_jdb(cpu))
    {
      bool online = Cpu::online(cpu);
      if (!online)
	{
	  printf("CPU %u is not online...\n", Cpu_number::val(cpu));
	  return;
	}
      printf("CPU %u has not entered JDB (try to display queue...\n", Cpu_number::val(cpu));
    }

  Item const *item = &Mp_request_queue::rq.cpu(cpu);
  Fifo const *fifo = &Mp_request_queue::fifo.cpu(cpu);


  printf("CPU[%2u]: Mp request item @%p, Mp request FIFO @%p\n"
         " Local queue Item:\n",
         Cpuz_number::val(cpu), item, fifo);

  print_request(item);

  printf(" Request FIFO: head = %p(%u), tail = { %p(%u) }\n",
         fifo->_head, find_cpu(fifo->_head),
	 fifo->_tail, find_cpu(fifo->_tail));

  item = fifo->_head;
  while (item)
    {
      print_request(item);
      item = item->next;
    }

  puts("");
}

PUBLIC
Jdb_module::Action_code
Jdb_mp_request_module::action (int cmd, void *&argbuf, char const *&fmt, int &next)
{
  char const *c = (char const *)argbuf;
  Cpu_number cpu;
  if (cmd!=0)
    return NOTHING;

  if (argbuf != &Cpu_number::val(cpu))
    {
      if (*c == 'a')
	Jdb::foreach_cpu(&print_queue);
      else if (*c >= '0' && *c <= '9')
	{
	  next = *c; argbuf = &cpu; fmt = "%i";
	  return EXTRA_INPUT_WITH_NEXTCHAR;
	}
    }
  else
    print_queue(cpu);

  return NOTHING;
}

PUBLIC
int
Jdb_mp_request_module::num_cmds() const
{ 
  return 1;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_mp_request_module::cmds() const
{
  static char c;
  static Cmd cs[] =
    { { 0, "", "mpqueue", "%C", "mpqueue all|<cpunum>\tdump X-CPU "
                                "request queues", &c } };

  return cs;
}

IMPLEMENT
Jdb_mp_request_module::Jdb_mp_request_module()
  : Jdb_module("INFO")
{}
