INTERFACE:

#include "types.h"

class Ipi
{
public:
  static void init(Cpu_number cpu);
};

INTERFACE[!mp]:

EXTENSION class Ipi
{
public:
  enum Message { Request, Global_request, Debug };
};

INTERFACE[mp]:

#include "per_cpu_data.h"
#include "spin_lock.h"

EXTENSION class Ipi
{
private:
  static Per_cpu<Ipi> _ipi;
};

// ------------------------------------------------------------------------
IMPLEMENTATION[!mp]:

IMPLEMENT inline
void
Ipi::init(Cpu_number cpu)
{ (void)cpu; }

PUBLIC static inline
void
Ipi::send(Message, Cpu_number from_cpu, Cpu_number to_cpu)
{ (void)from_cpu; (void)to_cpu; }

PUBLIC static inline
void
Ipi::eoi(Message, Cpu_number on_cpu)
{ (void)on_cpu; }

PUBLIC static inline
void
Ipi::bcast(Message, Cpu_number from_cpu)
{ (void)from_cpu; }


// ------------------------------------------------------------------------
IMPLEMENTATION[mp]:

DEFINE_PER_CPU Per_cpu<Ipi> Ipi::_ipi;

// ------------------------------------------------------------------------
IMPLEMENTATION[!(mp && debug)]:

PUBLIC static inline
void
Ipi::stat_sent(Cpu_number from_cpu)
{ (void)from_cpu; }

PUBLIC static inline
void
Ipi::stat_received(Cpu_number on_cpu)
{ (void)on_cpu; }

// ------------------------------------------------------------------------
IMPLEMENTATION[mp && debug]:

#include "atomic.h"
#include "globals.h"

EXTENSION class Ipi
{
private:
  friend class Jdb_ipi_module;
  Mword _stat_sent;
  Mword _stat_received;
};

PUBLIC static inline NEEDS["atomic.h"]
void
Ipi::stat_sent(Cpu_number from_cpu)
{ atomic_mp_add(&_ipi.cpu(from_cpu)._stat_sent, 1); }

PUBLIC static inline
void
Ipi::stat_received(Cpu_number on_cpu)
{ _ipi.cpu(on_cpu)._stat_received++; }
