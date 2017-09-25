
unsigned long long __umoddi3(unsigned long long, unsigned long long);
unsigned long long __udivdi3(unsigned long long, unsigned long long);

struct llmoddiv_t
{
  unsigned long long div;
  unsigned long long mod;
};

struct llmoddiv_t llmoddiv(unsigned long long div, unsigned long long s)
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


unsigned long long __umoddi3(unsigned long long div, unsigned long long s)
{ return llmoddiv(div,s).mod; }

unsigned long long __udivdi3(unsigned long long div, unsigned long long s)
{ return llmoddiv(div,s).div; }
