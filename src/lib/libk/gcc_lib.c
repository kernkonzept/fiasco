
unsigned long long __umoddi3(unsigned long long, unsigned long long);
unsigned long long __udivdi3(unsigned long long, unsigned long long);
unsigned long long __udivmoddi4(unsigned long long, unsigned long long,
                                unsigned long long *);

struct ullmoddiv_t
{
  unsigned long long div;
  unsigned long long mod;
};

/**
 * 64-bit unsigned integer division + modulo for 32-bit machines.
 * Used by the wrapper functions __umoddi3(), __udivdi3(), __udivmoddi4(), and
 * __aeabi_uldivmod().
 *
 * \param div  Dividend.
 * \param s    Divisor.
 * \returns ullmoddiv_t structure containing the divisor and modulo result from
 *          the 64-bit unsigned division.
 */
static struct ullmoddiv_t ullmoddiv(unsigned long long div, unsigned long long s)
{
  unsigned long long i;
  unsigned long long tmp;

  if (s == 0)
    return (struct ullmoddiv_t){.div = 0, .mod = 0};

  if (s > div)
    return (struct ullmoddiv_t){.div = 0, .mod = div};

  tmp = 1;
  while (!(s & (1ULL << 63)) && s < div)
    {
      s <<= 1;
      tmp <<= 1;
    }

  i = 0;
  while (1)
    {
      if (div >= s)
        {
          div -= s;
          i |= tmp;
        }
      if (div == 0)
        break;
      tmp >>= 1;
      if (!tmp)
        break;
      s >>= 1;
    }

  return (struct ullmoddiv_t){.div = i, .mod = div};
}

struct umoddiv_t
{
  unsigned div;
  unsigned mod;
};

/**
 * 32-bit unsigned integer division + modulo for 32-bit machines lacking the
 * `udiv` Assembler instruction.
 * Implemented analogously to ullmoddiv().
 * Used by the wrapper functions __aeabi_uidivmod() and __aeabi_uidiv().
 *
 * \param div  Dividend.
 * \param s    Divisor.
 * \returns umoddiv_t structure containing the divisor and modulo result from
 *          the 32-bit unsigned division.
 */
struct umoddiv_t umoddiv(unsigned div, unsigned s);
struct umoddiv_t umoddiv(unsigned div, unsigned s)
{
  unsigned i;
  unsigned tmp;

  if (s == 0)
    return (struct umoddiv_t){.div = 0, .mod = 0};

  if (s > div)
    return (struct umoddiv_t){.div = 0, .mod = div};

  tmp = 1;
  while (!(s & (1U << 31)) && s < div)
    {
      s <<= 1;
      tmp <<= 1;
    }

  i = 0;
  while (1)
    {
      if (div >= s)
        {
          div -= s;
          i |= tmp;
        }
      if (div == 0)
        break;
      tmp >>= 1;
      if (!tmp)
        break;
      s >>= 1;
    }

  return (struct umoddiv_t){.div = i, .mod = div};
}

/**
 * 64-bit unsigned modulo for 32-bit machines.
 *
 * \param div  Dividend.
 * \param s    Divisor.
 * \return div / s.
 */
unsigned long long __umoddi3(unsigned long long div, unsigned long long s)
{ return ullmoddiv(div, s).mod; }

/**
 * 64-bit unsigned division for 32-bit machines.
 *
 * \param div  Dividend.
 * \param s    Divisor.
 * \return div / s.
 */
unsigned long long __udivdi3(unsigned long long div, unsigned long long s)
{ return ullmoddiv(div, s).div; }

/**
 * 64-bit unsigned division + modulo for 32-bit machines.
 *
 * \param n       Dividend.
 * \param d       Divisor.
 * \param[out] r  Pointer to the result holding div % s.
 * \return div / s.
 */
unsigned long long __udivmoddi4(unsigned long long div, unsigned long long s,
                                unsigned long long *r)
{
  struct ullmoddiv_t md = ullmoddiv(div, s);
  if (r)
    *r = md.mod;
  return md.div;
}
