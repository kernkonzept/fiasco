
int strcmp(register const char *s,register const char *t) {

  register char x;

  for (;;)
    {
      x = *s;
      if (x != *t)
	break;
      if (!x)
	break;
      ++s; ++t;
    }
  return ((int)(unsigned int)(unsigned char) x)
       - ((int)(unsigned int)(unsigned char) *t);
}

int strcoll(const char *s,const char* t) __attribute__((weak,alias("strcmp")));
