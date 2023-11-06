
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

union llval_t
{
  struct {
#ifdef CONFIG_BIG_ENDIAN
    int high;
    unsigned low;
#else
    unsigned low;
    int high;
#endif
  };
  long long ll;
};

long long __ashldi3(long long, int);
long long __ashrdi3(long long, int);

/**
 * 64-bit left shift for 32-bit machines.
 *
 * \param[in] val   Value
 * \param[in] bits  Bits to shift
 *
 * \pre 0 <= bits <= 63
 * \return val << bits
 */
long long __ashldi3(long long val, int bits)
{
  union llval_t v;
  v.ll = val;

  if (bits >= 32)
    {
      v.high = v.low << (bits - 32);
      v.low = 0;
    }
  else if (bits != 0)
    {
      v.high = (v.high << bits) | (v.low >> (32 - bits));
      v.low = v.low << bits;
    }

  return v.ll;
}

/**
 * 64-bit right shift for 32-bit machines.
 *
 * \param[in] val   Value
 * \param[in] bits  Bits to shift
 *
 * \pre 0 <= bits <= 63
 * \return val >> bits
 */
long long __ashrdi3(long long val, int bits)
{
  union llval_t v;
  v.ll = val;

  if (bits >= 32)
    {
      v.low = v.high >> (bits - 32);
      v.high = v.high >> 31; // fill v.high with leftmost bit
    }
  else if (bits != 0)
    {
      v.low = (v.high << (32 - bits)) | (v.low >> bits);
      v.high = v.high >> bits;
    }

  return v.ll;
}

int __ffssi2(int val);
int __ffsdi2(long long val);
int __ctzsi2(unsigned val);
int __ctzdi2(unsigned long val);
int __clzsi2(unsigned val);
int __clzdi2(unsigned long val);

/**
 * Return the index of the least significant 1-bit in `val`, or zero if `val`
 * is zero. The least significant bit is index one.
 *
 * \param[in] val  Value
 *
 * \return Index of least significant 1-bit in val
 */
int __ffssi2(int val)
{
  for (unsigned i = 1; i <= 32; i++, val >>= 1)
    {
      if (val & 1)
        return i;
    }

  return 0;
}

/**
 * Return the index of the least significant 1-bit in `val`, or zero if `val`
 * is zero. The least significant bit is index one.
 *
 * \param[in] val  Value
 *
 * \return Index of least significant 1-bit in val
 */
int __ffsdi2(long long val)
{
  for (unsigned i = 1; i <= 64; i++, val >>= 1)
    {
      if (val & 1)
        return i;
    }

  return 0;
}

/**
 * Return the number of trailing 0-bits in `val`. This is actually the index of
 * the least significant 1-bit starting at 0. This is __ffssi2() minus 1.
 * Required by gcc for __builtin_ctz().
 *
 * \param[in] val  Value
 *
 * \return Number of trailing 0 bits in `val`.
 */
int __ctzsi2(unsigned val)
{
  if (!val)
    return 32;

  int c = 0;
  if ((val & 0xffff) == 0)
    {
      val >>= 16;
      c += 16;
    }
  if ((val & 0xff) == 0)
    {
      val >>= 8;
      c += 8;
    }
  if ((val & 0xf) == 0)
    {
      val >>= 4;
      c += 4;
    }
  if ((val & 0x3) == 0)
    {
      val >>= 2;
      c += 2;
    }
  if ((val & 0x1) == 0)
    ++c;

  return c;
}


/**
 * Return the number of trailing 0-bits in `val`. This is actually the index of
 * the least significant 1-bit starting at 0. This is __ffsdi2() minus 1.
 * Required by gcc for __builtin_ctzl().
 *
 * \param[in] val  Value
 *
 * \return Number of trailing 0 bits in `val`.
 */
int __ctzdi2(unsigned long val)
{
  int c = __ctzsi2(val & 0xffffffffU);
  if (c == 32)
    c += __ctzsi2(((unsigned long long)val) >> 32);
  return c;
}

/**
 * Return the number of leading 0-bits in `val`.
 * Required by gcc for __builtin_clz().
 *
 * \param[in] val  Value
 *
 * \return Number of trailing 0 bits in `val`.
 */
int __clzsi2(unsigned val)
{
  if (!val)
    return 32;

  int c = 0;
  if ((val & 0xffff0000) == 0)
    {
      val <<= 16;
      c += 16;
    }
  if ((val & 0xff000000) == 0)
    {
      val <<= 8;
      c += 8;
    }
  if ((val & 0xf0000000) == 0)
    {
      val <<= 4;
      c += 4;
    }
  if ((val & 0xc0000000) == 0)
    {
      val <<= 2;
      c += 2;
    }
  if ((val & 0x80000000) == 0)
    ++c;

  return c;
}

/**
 * Return the number of leading 0-bits in `val`.
 * Required by gcc for __builtin_clzl().
 *
 * \param[in] val  Value
 *
 * \return Number of trailing 0 bits in `val`.
 */
int __clzdi2(unsigned long val)
{
  int c = __clzsi2(((unsigned long long)val) >> 32);
  if (c == 32)
    c += __clzsi2(val & 0xffffffffU);
  return c;
}
