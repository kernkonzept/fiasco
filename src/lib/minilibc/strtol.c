#include <ctype.h>
#include <stdlib.h>

#if __SIZEOF_LONG__ == 8
#define ABS_LONG_MIN 0x8000000000000000UL
#else
#define ABS_LONG_MIN 0x80000000UL
#endif
#define LONG_MAX __LONG_MAX__
#define LONG_MIN (-LONG_MAX - 1L)

/**
 * A function to convert strings to long integers clamped at the target type
 * range.
 *
 * \param nptr    Pointer to the string to be converted.
 * \param endptr  Pointer to the location where to store the address of the
 *                character after the last valid parsed character. May be NULL.
 * \param base    The base of the number encoded by the string in nptr.
 *
 * \returns  The converted number or the largest/smallest representable number
 *           if an over- or underflow occurs.
 */
long int strtol(const char *nptr, char **endptr, int base) {

  int neg = 0;
  unsigned long int v;

  while (isspace (*nptr))
    nptr++;

  if (*nptr == '-') {
    neg = -1;
    ++nptr;
  }

  v = strtoul (nptr, endptr, base);

  if (v >= ABS_LONG_MIN) {
    if (v == ABS_LONG_MIN && neg) {
      return v;
    }
    return neg ? LONG_MIN : LONG_MAX;
  }

  return (neg ? -v : v);
}
