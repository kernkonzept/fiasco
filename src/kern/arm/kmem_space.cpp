INTERFACE [arm]:

#include "paging.h"
#include "mem_layout.h"

class Kmem_space : public Mem_layout
{
public:
  static void init();
  static void init_hw();
};

