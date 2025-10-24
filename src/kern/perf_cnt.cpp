INTERFACE:

#include "cpu.h"
#include "types.h"

class Perf_cnt_arch;

class Perf_cnt
{
public:
  enum {
    Max_slot = 2,
    Max_pmc  = 4,
  };

  enum Unit_mask_type
    { None, Fixed, Exclusive, Bitmask, };

  typedef Mword (*Perf_read_fn)();
};

// -----------------------------------------------------------------------
IMPLEMENTATION [!perf_cnt]:

PUBLIC static inline
void
Perf_cnt::get_unit_mask(Mword, Unit_mask_type *, Mword *, Mword *) {}

PUBLIC static inline
void
Perf_cnt::get_unit_mask_entry(Mword, Mword, Mword *, const char **) {}

PUBLIC static inline
void
Perf_cnt::get_perf_event(Mword, unsigned *, const char **, const char **) {}

PUBLIC static inline Mword Perf_cnt::get_max_perf_event() { return 0; }
PUBLIC static inline void  Perf_cnt::split_event(Mword, unsigned *, Mword *) {}
PUBLIC static inline Mword Perf_cnt::lookup_event(Mword) { return 0; }
PUBLIC static inline void  Perf_cnt::combine_event(Mword, Mword, Mword *) {}

PUBLIC static inline char const * Perf_cnt::perf_type() { return "nothing"; }

PUBLIC static inline
int
Perf_cnt::mode(Mword /*slot*/, const char **mode, const char **name,
		    Mword *event, Mword *user, Mword *kern, Mword *edge)
{
  *mode  = *name = "";
  *user  = *kern = *edge = *event = 0;
  return 0;
}

PUBLIC static inline
int
Perf_cnt::setup_pmc(Mword, Mword, Mword, Mword, Mword)
{
  return 0;
}

PUBLIC static inline void Perf_cnt::start_watchdog() {}
PUBLIC static inline void Perf_cnt::stop_watchdog() {}
PUBLIC static inline void Perf_cnt::touch_watchdog() {}
PUBLIC static inline int  Perf_cnt::have_watchdog() { return 0; }
PUBLIC static inline void Perf_cnt::setup_watchdog(Mword) {}
PUBLIC static inline void Perf_cnt::init_ap(Cpu const &) {}
