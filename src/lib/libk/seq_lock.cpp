INTERFACE:

#include "types.h"

/**
 * Sequence lock for data written by a single writer and read by readers on
 * any CPU.
 *
 * The writer never waits for readers. A reader detects a concurrent write and
 * retries:
 *
 * ```
 * // writer, at most one at a time, ensured by the caller
 * sl.lock_write();
 * write_now(&data, value);
 * sl.unlock_write();
 *
 * // reader
 * Mword seq;
 * do
 *   {
 *     seq = sl.read_begin();
 *     value = access_once(&data);
 *   }
 * while (sl.read_retry(seq));
 *
 * // or simply
 * value = sl.read([&](){ return access_once(&data); });
 * ```
 *
 * The counter is odd while a write is in progress. A read is consistent if
 * the counter was even before the read and did not change until after the
 * read. Wrap-around of the counter is harmless as it is only compared for
 * equality.
 *
 * The protected data must be accessed with access_once() and write_now(),
 * because a reader may see partially written data before it retries.
 *
 * \note A reader must not run on the CPU of the writer while the writer is
 *       between lock_write() and unlock_write(), for example in an interrupt
 *       handler, as it would never see an even counter.
 */
class Seq_lock
{
private:
  Mword _seq = 0;
};

IMPLEMENTATION:

#include "mem.h"
#include "processor.h"

/**
 * Start a write. The counter becomes odd.
 */
PUBLIC inline NEEDS["mem.h"]
void
Seq_lock::lock_write()
{
  write_now(&_seq, _seq + 1);
  // Order the counter update before the writes of the protected data.
  Mem::mp_wmb();
}

/**
 * Finish a write. The counter becomes even.
 */
PUBLIC inline NEEDS["mem.h"]
void
Seq_lock::unlock_write()
{
  // Order the writes of the protected data before the counter update.
  Mem::mp_wmb();
  write_now(&_seq, _seq + 1);
}

/**
 * Start a read.
 *
 * Waits until no write is in progress.
 *
 * \return Counter value to pass to read_retry().
 */
PUBLIC inline NEEDS["mem.h", "processor.h"]
Mword
Seq_lock::read_begin() const
{
  Mword seq;
  while ((seq = access_once(&_seq)) & 1)
    Proc::pause();

  // Order the counter read before the reads of the protected data.
  Mem::mp_rmb();
  return seq;
}

/**
 * Check whether a read must be repeated.
 *
 * \param seq  Counter value returned by read_begin().
 *
 * \retval true   A write happened during the read, the read must be repeated.
 * \retval false  The read data is consistent.
 */
PUBLIC inline NEEDS["mem.h"]
bool
Seq_lock::read_retry(Mword seq) const
{
  // Order the reads of the protected data before the counter read.
  Mem::mp_rmb();
  return access_once(&_seq) != seq;
}

/**
 * Read the protected data with `fct`.
 *
 * Calls `fct` again until no write happened during the call.
 *
 * \return What the last call of `fct` returned.
 */
PUBLIC template <typename Fct> inline
auto
Seq_lock::read(Fct &&fct) const
{
  for (;;)
    {
      Mword const seq = read_begin();
      auto const value = fct();
      if (!read_retry(seq))
        return value;
    }
}
