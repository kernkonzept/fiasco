INTERFACE:

#include "globalconfig.h"
#include "jdb_ktrace.h"
#include "l4_types.h"
#include "std_macros.h"
#include "tb_entry.h"
#include "spin_lock.h"

class Context;
class Log_event;
struct Tracebuffer_status;

class Jdb_tbuf
{
public:
  static void (*direct_log_entry)(Tb_entry*, const char*);

  enum
  {
    Event  = 1,
    Result = 2
  };

protected:
  static Tb_entry_union *_tbuf_act;	// current entry
  static Tb_entry_union *_tbuf_max;
  static Mword		_entries;	// number of occupied entries
  static Mword		_max_entries;	// maximum number of entries
  static Mword          _filter_enabled;// !=0 if filter is active
  static Mword		_number;	// current event number
  static Mword		_count_mask1;
  static Mword		_count_mask2;
  static Address        _size;		// size of memory area for tbuffer
  static Tracebuffer_status *_status;
  static Tb_entry_union *_buffer;
  static Spin_lock<>    _lock;
};

#ifdef CONFIG_JDB_LOGGING


#define END_LOG_EVENT						\
	}							\
    } while (0)

#else // ! CONFIG_JDB_LOGGING

#define BEGIN_LOG_EVENT(name, sc, fmt)				\
  if (0)							\
    { char __do_log__ = 0; (void)__do_log__;

#define END_LOG_EVENT						\
    }

#endif // ! CONFIG_JDB_LOGGING


IMPLEMENTATION:

#include "config.h"
#include "cpu_lock.h"
#include "initcalls.h"
#include "lock_guard.h"
#include "std_macros.h"

// read only: initialized at boot
Tracebuffer_status *Jdb_tbuf::_status;
Tb_entry_union *Jdb_tbuf::_buffer;
Address Jdb_tbuf::_size;
Mword Jdb_tbuf::_max_entries;
Tb_entry_union *Jdb_tbuf::_tbuf_max;
Mword Jdb_tbuf::_count_mask1;
Mword Jdb_tbuf::_count_mask2;

// read mostly (only modified in JDB)
Mword Jdb_tbuf::_filter_enabled;

// modified often (for each new entry)
Tb_entry_union *Jdb_tbuf::_tbuf_act;
Mword Jdb_tbuf::_entries;
Mword Jdb_tbuf::_number;
Spin_lock<> Jdb_tbuf::_lock;


static void direct_log_dummy(Tb_entry*, const char*)
{}

void (*Jdb_tbuf::direct_log_entry)(Tb_entry*, const char*) = &direct_log_dummy;

PUBLIC static inline Tracebuffer_status *Jdb_tbuf::status() { return _status; }
PROTECTED static inline Tb_entry_union *Jdb_tbuf::buffer() { return _buffer; }
PUBLIC static inline Address Jdb_tbuf::size() { return _size; }

/** Clear tracebuffer. */
PUBLIC static
void
Jdb_tbuf::clear_tbuf()
{
  Mword i;

  for (i = 0; i < _max_entries; i++)
    buffer()[i].clear();

  _tbuf_act = buffer();
  _entries = 0;
}

/** Return pointer to new tracebuffer entry. */
PUBLIC static
Tb_entry*
Jdb_tbuf::new_entry()
{
  Tb_entry *tb;
  {
    auto guard = lock_guard(_lock);

    tb = _tbuf_act;

    status()->current = (Address)tb;

    if (++_tbuf_act >= _tbuf_max)
      _tbuf_act = buffer();

    if (_entries < _max_entries)
      _entries++;

    tb->number(++_number);
  }

  tb->rdtsc();
  tb->rdpmc1();
  tb->rdpmc2();

  return tb;
}

PUBLIC template<typename T> static inline
T*
Jdb_tbuf::new_entry()
{
  static_assert(sizeof(T) <= sizeof(Tb_entry_union), "tb entry T too big");
  return static_cast<T*>(new_entry());
}

/** Commit tracebuffer entry. */
PUBLIC static
void
Jdb_tbuf::commit_entry()
{
  if (EXPECT_FALSE((_number & _count_mask2) == 0))
    {
      if (_number & _count_mask1)
	status()->window[0].version++; // 64-bit value!
      else
	status()->window[1].version++; // 64-bit value!

#if 0 // disbale Tbuf vIRQ for the time beeing (see bug #357)
      // fire the virtual 'buffer full' irq
      if (_observer)
        {
          auto guard = lock_guard(cpu_lock);
	  _observer->notify();
	}
#endif
    }
}

/** Return number of entries currently allocated in tracebuffer.
 * @return number of entries */
PUBLIC static inline
Mword
Jdb_tbuf::unfiltered_entries()
{
  return _entries;
}

PUBLIC static
Mword
Jdb_tbuf::entries()
{
  if (!_filter_enabled)
    return unfiltered_entries();

  Mword cnt = 0;

  for (Mword idx = 0; idx<unfiltered_entries(); idx++)
    if (!buffer()[idx].hidden())
      cnt++;

  return cnt;
}

/** Return maximum number of entries in tracebuffer.
 * @return number of entries */
PUBLIC static inline
Mword
Jdb_tbuf::max_entries()
{
  return _max_entries;
}

/** Set maximum number of entries in tracebuffer. */
PUBLIC static inline
void
Jdb_tbuf::max_entries (Mword num)
{
  _max_entries = num;
}

/** Check if event is valid.
 * @param idx position of event in tracebuffer
 * @return 0 if not valid, 1 if valid */
PUBLIC static inline
int
Jdb_tbuf::event_valid(Mword idx)
{
  return idx < _entries;
}

/** Return pointer to tracebuffer event.
 * @param  position of event in tracebuffer:
 *         0 is last event, 1 the event before and so on
 * @return pointer to tracebuffer event
 *
 * event with idx == 0 is the last event queued in
 * event with idx == 1 is the event before */
PUBLIC static
Tb_entry*
Jdb_tbuf::unfiltered_lookup(Mword idx)
{
  if (!event_valid(idx))
    return 0;

  Tb_entry_union *e = _tbuf_act - idx - 1;

  if (e < buffer())
    e += _max_entries;

  return e;
}

/** Return pointer to tracebuffer event.
 * Don't count hidden events.
 * @param  position of event in tracebuffer:
 *         0 is last event, 1 the event before and so on
 * @return pointer to tracebuffer event
 *
 * event with idx == 0 is the last event queued in
 * event with idx == 1 is the event before */
PUBLIC static
Tb_entry*
Jdb_tbuf::lookup(Mword look_idx)
{
  if (!_filter_enabled)
    return unfiltered_lookup(look_idx);

  for (Mword idx = 0;; idx++)
    {
      Tb_entry *e = unfiltered_lookup(idx);

      if (!e)
	return 0;
      if (e->hidden())
	continue;
      if (!look_idx--)
	return e;
    }
}

PUBLIC static
Mword
Jdb_tbuf::unfiltered_idx(Tb_entry const *e)
{
  Tb_entry_union const *ef = static_cast<Tb_entry_union const *>(e);
  Mword idx = _tbuf_act - ef - 1;

  if (idx > _max_entries)
    idx += _max_entries;

  return idx;
}

/** Tb_entry => tracebuffer index. */
PUBLIC static
Mword
Jdb_tbuf::idx(Tb_entry const *e)
{
  if (!_filter_enabled)
    return unfiltered_idx(e);

  Tb_entry_union const *ef = static_cast<Tb_entry_union const*>(e);
  Mword idx = (Mword) - 1;

  for (;;)
    {
      if (!ef->hidden())
	idx++;
      ef++;
      if (ef >= buffer() + _max_entries)
	ef -= _max_entries;
      if (ef == _tbuf_act)
	break;
    }

  return idx;
}

/** Event number => Tb_entry. */
PUBLIC static inline
Tb_entry*
Jdb_tbuf::search(Mword nr)
{
  Tb_entry *e;

  for (Mword idx = 0; (e = unfiltered_lookup(idx)); idx++)
    if (e->number() == nr)
      return e;

  return 0;
}

/** Event number => tracebuffer index.
 * @param  nr  number of event
 * @return tracebuffer index of event which has the number nr or
 *         -1 if there is no event with this number or
 *         -2 if the event is currently hidden. */
PUBLIC static
Mword
Jdb_tbuf::search_to_idx(Mword nr)
{
  if (nr == (Mword) - 1)
    return (Mword) - 1;

  Tb_entry *e;

  if (!_filter_enabled)
    {
      e = search(nr);
      if (!e)
	return (Mword) - 1;
      return unfiltered_idx(e);
    }

  for (Mword idx_u = 0, idx_f = 0; (e = unfiltered_lookup(idx_u)); idx_u++)
    {
      if (e->number() == nr)
	return e->hidden() ? (Mword) - 2 : idx_f;

      if (!e->hidden())
	idx_f++;
    }

  return (Mword)-1;
}

/** Return some information about log event.
 * @param idx number of event to determine the info
 * @retval number event number
 * @retval tsc event value of CPU cycles
 * @retval pmc event value of perf counter cycles
 * @return 0 if something wrong, 1 if everything ok */
PUBLIC static
int
Jdb_tbuf::event(Mword idx, Mword *number, Unsigned32 *kclock,
		Unsigned64 *tsc, Unsigned32 *pmc1, Unsigned32 *pmc2)
{
  Tb_entry *e = lookup(idx);

  if (!e)
    return false;

  *number = e->number();
  if (kclock)
    *kclock = e->kclock();
  if (tsc)
    *tsc = e->tsc();
  if (pmc1)
    *pmc1 = e->pmc1();
  if (pmc2)
    *pmc2 = e->pmc2();
  return true;
}

/** Get difference CPU cycles between event idx and event idx+1 on the same CPU.
 * @param idx position of first event in tracebuffer
 * @retval difference in CPU cycles
 * @return 0 if something wrong, 1 if everything ok */
PUBLIC static
int
Jdb_tbuf::diff_tsc(Mword idx, Signed64 *delta)
{
  Tb_entry *e      = lookup(idx);
  Tb_entry *e_prev;

  if (!e)
    return false;

  do
    {
      e_prev = lookup(++idx);
      if (!e_prev)
        return false;
    }
  while (e->cpu() != e_prev->cpu());

  *delta = e->tsc() - e_prev->tsc();
  return true;
}

/** Get difference perfcnt cycles between event idx and event idx+1.
 * @param idx position of first event in tracebuffer
 * @param nr  number of perfcounter (0=first, 1=second)
 * @retval difference in perfcnt cycles
 * @return 0 if something wrong, 1 if everything ok */
PUBLIC static
int
Jdb_tbuf::diff_pmc(Mword idx, Mword nr, Signed32 *delta)
{
  Tb_entry *e      = lookup(idx);
  Tb_entry *e_prev = lookup(idx + 1);

  if (!e || !e_prev)
    return false;

  switch (nr)
    {
    case 0: *delta = e->pmc1() - e_prev->pmc1(); break;
    case 1: *delta = e->pmc2() - e_prev->pmc2(); break;
    }

  return true;
}

PUBLIC static inline
void
Jdb_tbuf::enable_filter()
{
  _filter_enabled = 1;
}

PUBLIC static inline
void
Jdb_tbuf::disable_filter()
{
  _filter_enabled = 0;
}
