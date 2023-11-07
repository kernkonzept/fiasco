INTERFACE:

#include <cxx/cxx_int>
#include "config.h"

/**
 * Page address manipulation helpers.
 *
 * \tparam PAGE_SHIFT  Page width in bits.
 */
template<unsigned int PAGE_SHIFT>
class Paging_bits
{
public:
  /**
   * Round to the nearest higher page.
   *
   * \param value  Value to round.
   *
   * \return Nearest higher page of \ref value.
   */
  template<typename TYPE>
  static constexpr
  cxx::enable_if_t<cxx::is_integral_v<TYPE>, TYPE>
  round(TYPE const &value)
  {
    return cxx::ceil_lsb(value, PAGE_SHIFT);
  }

  /**
   * Round to the nearest higher page.
   *
   * This variant accepts enum constants and returns the address in the
   * underlying type.
   *
   * \param value  Value to round.
   *
   * \return Nearest higher page of \ref value.
   */
  template<typename TYPE>
  static constexpr
  cxx::enable_if_t<cxx::is_enum_v<TYPE>, cxx::underlying_type_t<TYPE>>
  round(TYPE const &value)
  {
    return cxx::ceil_lsb(value, PAGE_SHIFT);
  }

  /**
   * Truncate to the nearest lower page.
   *
   * \param value  Value to truncate.
   *
   * \return Nearest lower page of \ref value.
   */
  template<typename TYPE>
  static constexpr
  cxx::enable_if_t<cxx::is_integral_v<TYPE>, TYPE>
  trunc(TYPE const &value)
  {
    return cxx::mask_lsb(value, PAGE_SHIFT);
  }

  /**
   * Truncate to the nearest lower page.
   *
   * This variant accepts enum constants and returns the address in the
   * underlying type.
   *
   * \param value  Value to truncate.
   *
   * \return Nearest lower page of \ref value.
   */
  template<typename TYPE>
  static constexpr
  cxx::enable_if_t<cxx::is_enum_v<TYPE>, cxx::underlying_type_t<TYPE>>
  trunc(TYPE const &value)
  {
    return cxx::mask_lsb(value, PAGE_SHIFT);
  }

  /**
   * Get the offset within page.
   *
   * \param value  Value to get page offset of.
   *
   * \return Page offset of \ref value.
   */
  template<typename TYPE>
  static constexpr
  cxx::enable_if_t<cxx::is_integral_v<TYPE>, TYPE>
  offset(TYPE const &value)
  {
    return cxx::get_lsb(value, PAGE_SHIFT);
  }

  /**
   * Get the offset within page.
   *
   * This variant accepts enum constants and returns the offset in the
   * underlying type.
   *
   * \param value  Value to get page offset of.
   *
   * \return Page offset of \ref value.
   */
  template<typename TYPE>
  static constexpr
  cxx::enable_if_t<cxx::is_enum_v<TYPE>, cxx::underlying_type_t<TYPE>>
  offset(TYPE const &value)
  {
    return cxx::get_lsb(value, PAGE_SHIFT);
  }

  /**
   * Check whether the value is page-aligned,
   *
   * \param value  Value to check.
   *
   * \retval True if the \ref value is page-aligned.
   * \retval False if the \ref value is not page-aligned.
   */
  template<typename TYPE>
  static constexpr bool aligned(TYPE const &value)
  {
    return offset(value) == 0U;
  }

  /**
   * Get the truncated count of pages for the value.
   *
   * If the \ref value is not page-aligned, then the remainder is ignored.
   *
   * \param value  Address or size (in bytes).
   *
   * \return Truncated count of pages corresponding to \ref value.
   */
  template<typename TYPE>
  static constexpr TYPE count(TYPE const &value)
  {
    return value >> PAGE_SHIFT;
  }

  /**
   * Get the size of the given count of pages.
   *
   * \param superpages  Count of pages.
   *
   * \return Size of \ref pages in bytes.
   */
  template<typename TYPE>
  static constexpr TYPE size(TYPE const &pages)
  {
    return pages << PAGE_SHIFT;
  }
};

using Pg = Paging_bits<Config::PAGE_SHIFT>;
using Super_pg = Paging_bits<Config::SUPERPAGE_SHIFT>;
