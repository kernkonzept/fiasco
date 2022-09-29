
#include "construction.h"
#include "types.h"
#include "per_node_data.h"

typedef void (*ctor_t)(void);

static DECLARE_PER_NODE Per_node_data<int> construction_done;

void run_ctor_functions(ctor_function_t *start, ctor_function_t *end)
{
  if (start < end)
    {
      for (;start != end; ++start)
        (*start)();
    }
  else
    {
      for (;start != end;)
        {
          --start;
          (*start)();
        }
    }
}

void static_construction()
{
  extern ctor_function_t __INIT_ARRAY_START__[];
  extern ctor_function_t __INIT_ARRAY_END__[];
  run_ctor_functions(__INIT_ARRAY_START__, __INIT_ARRAY_END__);

  extern ctor_function_t __CTOR_END__[];
  extern ctor_function_t __CTOR_LIST__[];
  run_ctor_functions(__CTOR_LIST__, __CTOR_END__);

  *construction_done = 1;
}

void static_destruction()
{
  if(!*construction_done)
    return;

  extern ctor_function_t __DTOR_END__[];
  extern ctor_function_t __DTOR_LIST__[];
  run_ctor_functions(__DTOR_LIST__, __DTOR_END__);
}
