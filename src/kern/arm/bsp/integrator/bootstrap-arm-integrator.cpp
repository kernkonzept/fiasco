INTERFACE [arm && pf_integrator]:

EXTENSION class Bootstrap
{
public:
  enum { Cache_flush_area = 0xe0000000 };
};
