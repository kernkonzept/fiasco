INTERFACE [arm && pf_bcm283x]:

EXTENSION class Bootstrap
{
public:
  enum { Cache_flush_area = 0xe0000000 };
};
