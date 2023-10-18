#include <ctype.h>
#include <stdlib.h>

#define ABS_LONG_MIN 2147483648UL

/**
 * A function to convert strings to 32-bit integers.
 *
 * This function mimics the functionality of POSIX strtol but is limited to
 * converting to 32-bit values. Contrary to POSIX strtol it does not clamp the
 * returned value to the target value range but returns 0 in cases where the
 * number represented by nptr exceeds the 32-bit signed value range.
 *
 * \param nptr    Pointer to the string to be converted.
 * \param endptr  Pointer to the location where to store the address of the
 *                character after the last valid parsed character. May be NULL.
 * \param base    The base of the number encoded by the string in nptr.
 *
 * \ret  The converted number or 0 if the number exceeded the allowed value
 *       range.
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
    return 0; //(neg ? LONG_MIN : LONG_MAX);
  }

  return (neg ? -v : v);
}
