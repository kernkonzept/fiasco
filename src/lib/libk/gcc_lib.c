
unsigned long long __umoddi3(unsigned long long, unsigned long long);
unsigned long long __udivdi3(unsigned long long, unsigned long long);
unsigned long long __udivmoddi4(unsigned long long, unsigned long long,
                                unsigned long long *);

struct llmoddiv_t
{
  unsigned long long div;
  unsigned long long mod;
};

static struct llmoddiv_t llmoddiv(unsigned long long div, unsigned long long s)
{
  unsigned long long i;
  unsigned long long tmp;

  if (s == 0)
    return (struct llmoddiv_t){.div = 0, .mod = 0};

  if (s > div)
    return (struct llmoddiv_t){.div = 0, .mod = div};

  tmp = 1;
  while (!(s & (1ULL<<63)) && s < div)
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

  return (struct llmoddiv_t){.div = i, .mod = div};
}


/**
 * 64-bit unsigned modulo for 32-bit machines.
 *
 * \param div  Dividend.
 * \param s    Divisor.
 * \return div / s.
 */
unsigned long long __umoddi3(unsigned long long div, unsigned long long s)
{ return llmoddiv(div, s).mod; }

/**
 * 64-bit unsigned division for 32-bit machines.
 *
 * \param div  Dividend.
 * \param s    Divisor.
 * \return div / s.
 */
unsigned long long __udivdi3(unsigned long long div, unsigned long long s)
{ return llmoddiv(div, s).div; }

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
  struct llmoddiv_t md = llmoddiv(div, s);
  if (r)
    *r = md.mod;
  return md.div;
}
