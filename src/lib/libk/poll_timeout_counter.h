#pragma once

namespace L4 {

/**
 * Evaluate an expression for a maximum number of times.
 *
 * A typical use case is testing for a bit change in a hardware register for a
 * maximum number of times (polling). For example:
 *
 * \code{c++}
 * Mmio_register_block regs;
 * Poll_timeout_counter i(3000000);
 * while (i.test(!(regs.read<l4_uint32_t>(0x04) & 1)))
 *   ;
 * \endcode
 *
 * The following usage is \b wrong:
 * \code{c++}
 * ...
 * Poll_timeout_counter i(3000000);
 * while (!i.test((regs.read<l4_uint32_t>(0x04) & 1)))
 *   ;
 * \endcode
 *
 * This loop would never terminate if the hardware register doesn't change!
 */
class Poll_timeout_counter
{
public:
  /**
   * Constructor.
   *
   * \param counter_val  Maximum number of times to repeat the test.
   */
  Poll_timeout_counter(unsigned counter_val)
  {
    set(counter_val);
  }

  /**
   * Set the counter to a certain value.
   *
   * \param counter_val  New counter value for maximum number of times to
   *                     repeat the test.
   */
  void set(unsigned counter_val)
  {
    _c = counter_val;
  }

  /**
   * Evaluate the expression for a maximum number of times.
   */
  bool test(bool expression = true)
  {
    if (!expression)
      return false;

    if (_c)
      {
        --_c;
        return true;
      }

    return false;
  }

  /**
   * Indicator if the maximum number of tests was required.
   *
   * \retval true, if the maximum number of tests was required or if the
   *               counter was initialized to zero.
   */
  bool timed_out() const { return _c == 0; }

private:
  unsigned _c;
};

}
