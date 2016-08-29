INTERFACE [arm && bcm2835]:

EXTENSION class Bootstrap
{
public:
  enum { Cache_flush_area = 0xe0000000 };
};
