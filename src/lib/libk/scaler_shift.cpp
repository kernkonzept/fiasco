INTERFACE:

#include "types.h"

struct Scaler_shift
{
  Unsigned32 scaler;
  Unsigned32 shift;

  /**
   * Transform the given value.
   *
   * \param value  Input value
   *
   * \return  Transformed value.
   */
  Unsigned64 transform(Unsigned64 value) const;
};

IMPLEMENTATION:

/**
 * Determine scaling factor and shift value used for transforming a time stamp
 * (timer value) into a time value (microseconds or nanoseconds) or a time value
 * into a time stamp.
 *
 * \param from_freq     Source frequency.
 * \param to_freq       Target frequency.
 * \param scaler_shift  Determined scaling factor and shift value.
 *
 * To determine scaler/shift for converting an arbitrary timer frequency into
 * microseconds, use `to_freq=1'000'000` and `from_freq=<timer frequency>`. To
 * determine scaler/shift for converting microseconds into timer ticks, use
 * `to_freq=<timer frequency>` and `from_freq=1'000'000>`.
 *
 * The following formula is used to translate a time value `tv_from` into a time
 * value `tv_to` using `scaler` and `shift`:
 *
 * \code
 *              tv_from * scaler                 tv_from * scaler
 *   tv_to  =  ------------------ * 2^shift  =  ------------------
 *                    2^32                         2^(32-shift)
 * \endcode
 *
 * The shift value is important for low timer frequencies to keep a sane amount
 * of usable digits.
 */
PUBLIC static inline
Scaler_shift
Scaler_shift::calc(Unsigned32 from_freq, Unsigned32 to_freq)
{
  Unsigned32 shift = 0;
  while ((to_freq / (1 << shift)) / from_freq > 0)
    ++shift;
  Unsigned32 scaler = (((1ULL << 32) / (1ULL << shift)) * to_freq) / from_freq;
  return Scaler_shift{scaler, shift};
}
