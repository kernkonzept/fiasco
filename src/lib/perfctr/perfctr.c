#include <stdio.h>
#include "libperfctr.h"
#include "event_set.h"

/* current event set depending on CPU type */
static const struct perfctr_event_set *event_set;

static const struct perfctr_event*
perfctr_lookup_event_set(const struct perfctr_event_set *event_set,
			 unsigned evntsel, unsigned *nr)
{
  unsigned i;

  if (event_set->include)
    {
      const struct perfctr_event *event;

      if (0 != (event = perfctr_lookup_event_set(event_set->include, 
						 evntsel, nr)))
	return event;
    }
  for (i=0; i<event_set->nevents; i++)
    {
      if (event_set->events[i].evntsel == evntsel)
	{
	  *nr += i;
	  return (event_set->events + i);
	}
    }

  *nr += event_set->nevents;
  return 0;
}

static const struct perfctr_event*
perfctr_index_event_set(const struct perfctr_event_set *event_set,
			unsigned *nr)
{
  if (event_set->include)
    {
      const struct perfctr_event *event;

      if ((event = perfctr_index_event_set(event_set->include, nr)) != 0)
	return event;
    }
  if (*nr < event_set->nevents)
    return event_set->events + *nr;

  *nr -= event_set->nevents;
  return 0;
}

static unsigned
perfctr_get_max_event_set(const struct perfctr_event_set *event_set)
{
  unsigned nr = 0;

  if (event_set->include)
      nr += perfctr_get_max_event_set(event_set->include);

  return nr + event_set->nevents;
}

void
perfctr_set_cputype(unsigned cpu_type)
{
  event_set = perfctr_cpu_event_set(cpu_type);
}

const struct perfctr_event*
perfctr_lookup_event(unsigned evntsel, unsigned *nr)
{
  *nr = 0;
  return perfctr_lookup_event_set(event_set, evntsel, nr);
}

const struct perfctr_event*
perfctr_index_event(unsigned nr)
{
  return perfctr_index_event_set(event_set, &nr);
}

unsigned
perfctr_get_max_event(void)
{
  return perfctr_get_max_event_set(event_set);
}

