INTERFACE [arm && pf_ls1021a]:

EXTENSION class Bootstrap
{
public:
  enum { Cache_flush_area = 0xe0000000 };
};
