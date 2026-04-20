INTERFACE:

#include "globalconfig.h"
#include "jdb_ktrace.h"
#include "l4_types.h"
#include "std_macros.h"
#include "tb_entry.h"
#include "spin_lock.h"
#include "config.h"
#include "minmax.h"
#include "arithmetic.h"

class Context;
class Log_event;

struct alignas(cxx::bit_ceil(sizeof(Tb_entry_union))) Tracebuffer_slot
{
  // The slot sequence number is a member of the data.
  Tb_entry_union data;
};

class Jdb_tbuf
{
public:
  using Predicate = bool (*)(Tb_entry &);

  static constexpr Predicate all = [](Tb_entry &) { return true; };
  static constexpr Predicate filter_hidden
    = [](Tb_entry &entry) { return !entry.is_hidden(); };

  enum
  {
    Event  = 1,
    Result = 2
  };

  enum : Tb_sequence
  {
    Nil = 0U
  };

  enum : size_t
  {
    Hidden = ~size_t{0U} - 1,
    Not_found = ~size_t{0U}
  };

  enum : unsigned
  {
    Tracebuffer_version = 0
  };

  static void (*direct_log_entry)(Tb_entry *, const char *);

protected:
  static size_t _capacity;     //< Number of entries.
  static size_t _mask;         //< Mask for determining the slot index.
  static bool _filter_hidden;  //< Filtering of hidden entries enabled.
  static Tracebuffer_status *_status;
  static Tracebuffer_slot *_slots;
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

#include "atomic.h"
#include "config.h"
#include "cpu_lock.h"
#include "initcalls.h"
#include "lock_guard.h"
#include "mem.h"
#include "std_macros.h"

// Read only (initialized at boot).
size_t Jdb_tbuf::_capacity;
size_t Jdb_tbuf::_mask;
Tracebuffer_status *Jdb_tbuf::_status;
Tracebuffer_slot *Jdb_tbuf::_slots;

// Read mostly (only modified in JDB).
bool Jdb_tbuf::_filter_hidden;

static void direct_log_dummy(Tb_entry *, const char *)
{}

void (*Jdb_tbuf::direct_log_entry)(Tb_entry *, const char *) = &direct_log_dummy;

PUBLIC static inline Tracebuffer_status *Jdb_tbuf::status() { return _status; }
PUBLIC static inline Tracebuffer_slot *Jdb_tbuf::slots() { return _slots; }

PUBLIC static inline size_t Jdb_tbuf::size(size_t entries)
{ return entries * sizeof(Tracebuffer_slot); }

PUBLIC static inline size_t Jdb_tbuf::size() { return size(_capacity); }

PUBLIC static inline size_t Jdb_tbuf::capacity(size_t size)
{ return size / sizeof(Tracebuffer_slot); }

PUBLIC static inline size_t Jdb_tbuf::capacity()
{ return _capacity; }

/** Clear tracebuffer. */
PUBLIC static
void
Jdb_tbuf::clear_tbuf()
{
  for (size_t i = 0; i < _capacity; ++i)
    _slots[i].data.clear();

  _status->tail = Nil;
}

/**
 * Return pointer to the next tracebuffer entry.
 *
 * \param[out] seq  Sequence number of the next tracebuffer entry.
 *
 * \return Pointer to the next tracebuffer entry.
 */
PUBLIC static inline NEEDS["atomic.h", "mem.h"]
Tb_entry *
Jdb_tbuf::next_entry(Tb_sequence &seq)
{
  seq = atomic_add_fetch(&_status->tail, 1);
  Tb_entry *tb = &_slots[seq & _mask].data;

  tb->number(Nil);
  Mem::barrier();

  tb->rdtsc();
  tb->rdpmc1();
  tb->rdpmc2();

  return tb;
}

PUBLIC static inline
template<typename T>
T *
Jdb_tbuf::next_entry(Tb_sequence &seq)
{
  static_assert(sizeof(T) <= sizeof(Tb_entry_union));
  return static_cast<T *>(next_entry(seq));
}

/**
 * Commit tracebuffer entry.
 *
 * This method is executed when the entry is complete.
 *
 * \param entry  Tracebuffer entry to commit.
 * \param seq    Sequence number of the tracebuffer entry to commit.
 */
PUBLIC static inline
void
Jdb_tbuf::commit_entry(Tb_entry *entry, Tb_sequence seq)
{
  assert(seq != Nil);
  entry->number(seq);
  Mem::barrier();
}

/**
 * Return number of entries currently committed in tracebuffer.
 *
 * \tparam predicate  Filtering predicate (e.g. #filter_hidden).
 *
 * \return Number of entries currentry committed in tracebuffer (considering
 *         the filtering predicate).
 */
PUBLIC static
template<Jdb_tbuf::Predicate predicate>
size_t
Jdb_tbuf::entries()
{
  size_t cnt = 0;
  for (size_t i = 0; i < _capacity; ++i)
    {
      Tb_entry &data = _slots[i].data;
      if (data.number() != Nil && predicate(data))
        ++cnt;
    }

  return cnt;
}

/**
 * Check if entry is valid and committed.
 *
 * \param seq  Sequence number of the entry.
 *
 * \return Pointer to the entry with the given sequence number, if it is valid.
 * \retval nullptr  The entry with the given sequence number is not valid.
 */
PUBLIC static inline
Tb_entry *
Jdb_tbuf::entry_valid(Tb_sequence seq)
{
  assert(seq != Nil);

  Tb_entry &data = _slots[seq & _mask].data;
  if (data.number() == seq)
    return &data;

  return nullptr;
}

/**
 * Get pointer to a tracebuffer event.
 *
 * \tparam predicate  Filtering predicate (e.g. #filter_hidden).
 *
 * \param pos  Position of the event in the tracebuffer. 0 represents the last
 *             event, 1 represents the penultimate event, etc. The filtering
 *             predicate is taken into account.
 *
 * \return Pointer to the tracebuffer event with the given position (with
 *         the filtering predicate taken into account).
 * \retval nullptr  No tracebuffer event with the given position (with
 *                  the filtering predicate taken into account) found.
 */
PUBLIC static
template<Jdb_tbuf::Predicate predicate>
Tb_entry *
Jdb_tbuf::lookup(size_t pos)
{
  Tb_sequence seq = _status->tail;
  size_t bound = seq > _capacity ? seq - _capacity : 0;
  size_t cnt = pos;

  while (seq > bound)
    {
      Tb_entry *entry = entry_valid(seq);

      // Skip invalid entries and entries that are filtered out by the
      // predicate.
      if (!entry || !predicate(*entry))
        {
          --seq;
          continue;
        }

      // Found the entry with the desired position.
      if (cnt == 0)
        return entry;

      // Entry found, but not on the desired position.
      --seq;
      --cnt;
    }

  // No more entries.
  return nullptr;
}

/**
 * Get pointer to a tracebuffer event (possibly with filtering).
 *
 * \param pos  Position of the event in the tracebuffer. 0 represents the last
 *             event, 1 represents the penultimate event, etc. The current
 *             filtering setting is taken into account.
 *
 * \return Pointer to the tracebuffer event with the given position (with
 *         the current filtering setting taken into account).
 * \retval nullptr  No tracebuffer event with the given position (with
 *                  the current filtering setting taken into account) found.
 */
PUBLIC static
Tb_entry *
Jdb_tbuf::lookup(size_t pos)
{
  if (_filter_hidden)
    return lookup<filter_hidden>(pos);

  return lookup<all>(pos);
}

/**
 * Get the position of the given tracebuffer event.
 *
 * \tparam predicate  Filtering predicate (e.g. #filter_hidden).
 *
 * \param entry  Tracebuffer event to find in the tracebuffer. The filtering
 *               predicate is taken into account.
 *
 * \return Position of the given event in the tracebuffer (with the filtering
 *         predicate taken into account). 0 represents the last event,
 *         1 represents the penultimate event, etc.
 * \retval Not_found  The event is not present or no longer present in the
 *                    tracebuffer.
 */
PUBLIC static
template<Jdb_tbuf::Predicate predicate>
size_t
Jdb_tbuf::pos(Tb_entry const *event)
{
  Tb_sequence seq = _status->tail;
  size_t bound = seq > _capacity ? seq - _capacity : 0;
  size_t pos = 0;

  while (seq > bound)
    {
      Tb_entry *cur = entry_valid(seq);

      // Skip invalid entries and entries that are filtered out by the
      // predicate.
      if (!cur || !predicate(*cur))
        {
          --seq;
          continue;
        }

      // Found the desired entry.
      if (cur == event)
        return pos;

      // Entry found, but not the desired one.
      --seq;
      ++pos;
    }

  // No more entries.
  return Not_found;
}

/**
 * Get the position of the given tracebuffer event (possibly with filtering).
 *
 * \param entry  Tracebuffer event to find in the tracebuffer. The current
 *               filtering setting is taken into account.
 *
 * \return Position of the given event in the tracebuffer (with the current
 *         filtering setting taken into account). 0 represents the last event,
 *         1 represents the penultimate event, etc.
 * \retval Not_found  The event is not present or no longer present in the
 *                    tracebuffer.
 */
PUBLIC static
size_t
Jdb_tbuf::pos(Tb_entry const *event)
{
  if (_filter_hidden)
    return pos<filter_hidden>(event);

  return pos<all>(event);
}

/**
 * Get tracebuffer entry based on sequence number.
 *
 * \param seq  Sequence number of the tracebuffer entry.
 *
 * \return Tracebuffer entry with the given sequence number.
 * \retval nullptr  The event is not present or no longer present in the
 *                  tracebuffer.
 */
PUBLIC static inline
Tb_entry *
Jdb_tbuf::search(Tb_sequence seq)
{
  Tb_entry *tb = &_slots[seq & _mask].data;
  if (tb->number() == seq)
    return tb;

  return nullptr;
}

/**
 * Get tracebuffer entry position based on sequence number.
 *
 * \param seq  Sequence number of the tracebuffer entry.
 *
 * \return Position of the given event in the tracebuffer (with current
 *         filtering setting taken into account). 0 represents the last event,
 *         1 represents the penultimate event, etc.
 * \retval Not_found  The event is not present or no longer present in the
 *                    tracebuffer.
 * \retval Hidden     The event is present, but hidden.
 */
PUBLIC static
size_t
Jdb_tbuf::search_to_pos(Tb_sequence seq)
{
  Tb_entry *entry = search(seq);
  if (!entry)
    return Not_found;

  if (!_filter_hidden)
    return pos<all>(entry);

  if (entry->is_hidden())
    return Hidden;

  return pos<filter_hidden>(entry);
}

/**
 * Return some information about log event (possibly with filtering).
 *
 * \param      pos     Position of the even in the tracebuffer. 0 represents
 *                     the last event, 1 represents the penultimate event, etc.
 *                     The current filtering setting is taken into account.
 * \param[out] number  Event number.
 * \param[out] klock   Event value of kernel clock.
 * \param[out] tsc     Event value of CPU cycles.
 * \param[out] pmc1    Event value of perf counter 1 cycles.
 * \param[out] pmc2    Event value of perf counter 2 cycles.
 *
 * \retval false  Something went wrong.
 * \retval true   Everything is OK.
 */
PUBLIC static
bool
Jdb_tbuf::event(size_t pos, Tb_sequence *number, Unsigned32 *kclock,
                Unsigned64 *tsc, Unsigned32 *pmc1, Unsigned32 *pmc2)
{
  Tb_entry *entry = lookup(pos);
  if (!entry)
    return false;

  *number = entry->number();

  if (kclock)
    *kclock = entry->kclock();

  if (tsc)
    *tsc = entry->tsc();

  if (pmc1)
    *pmc1 = entry->pmc1();

  if (pmc2)
    *pmc2 = entry->pmc2();

  return true;
}

/**
 * Get the difference of CPU cycles of the given event and the previous event
 * on the same CPU (possibly with filtering).
 *
 * \param      pos    Position of the even in the tracebuffer. 0 represents
 *                    the last event, 1 represents the penultimate event, etc.
 *                    The current filtering setting is taken into account.
 * \param[out] delta  Difference in CPU cycles.
 *
 * \retval false  Something went wrong.
 * \retval true   Everything is OK.
 */
PUBLIC static
bool
Jdb_tbuf::diff_tsc(size_t pos, Signed64 *delta)
{
  Tb_entry *entry = lookup(pos);
  if (!entry)
    return false;

  Tb_entry *prev;

  do
    {
      prev = lookup(++pos);
      if (!prev)
        return false;
    }
  while (entry->cpu() != prev->cpu());

  *delta = entry->tsc() - prev->tsc();
  return true;
}

/**
 * Get the difference of performance counter cycles of the given event and the
 * previous event (possibly with filtering).
 *
 * \param      pos    Position of the even in the tracebuffer. 0 represents
 *                    the last event, 1 represents the penultimate event, etc.
 *                    The current filtering setting is taken into account.
 * \param      index  Performance counter index. 0 represents the first
 *                    counter, 1 represents the second counter.
 * \param[out] delta  Difference in performance counter cycles.
 *
 * \retval false  Something went wrong.
 * \retval true   Everything is OK.
 */
PUBLIC static
bool
Jdb_tbuf::diff_pmc(size_t pos, unsigned index, Signed32 *delta)
{
  Tb_entry *entry = lookup(pos);
  if (!entry)
    return false;

  Tb_entry *prev = lookup(pos + 1);
  if (!prev)
    return false;

  switch (index)
    {
    case 0:
      *delta = entry->pmc1() - prev->pmc1();
      break;
    case 1:
      *delta = entry->pmc2() - prev->pmc2();
      break;
    }

  return true;
}

PUBLIC static inline
void
Jdb_tbuf::hide_hidden()
{ _filter_hidden = true; }

PUBLIC static inline
void
Jdb_tbuf::show_hidden()
{ _filter_hidden = false; }
