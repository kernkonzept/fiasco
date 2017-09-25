INTERFACE [ppc32]:

#include "types.h"
#include "ppc_types.h"

EXTENSION class Boot_info
{
  public:
    /**
     * Return memory-mapped base address of uart/pic
     */
    static Address uart_base();
    static Address pic_base();

  private:
    static Address _prom_ptr;
};


//------------------------------------------------------------------------------
IMPLEMENTATION [ppc32]:

#include "boot_info.h"
#include <string.h>

Address Boot_info::_prom_ptr;

PUBLIC static
Of_device *
Boot_info::get_device(const char *name, const char *type)
{
  (void)name; (void)type;
#if 0
  Of_device *dev = reinterpret_cast<Of_device *>(_mbi.drives_addr);

  for (Unsigned32 i = 0; i < _mbi.drives_length/sizeof(Of_device); i ++)
    if ((!type || !strncmp(type, (dev + i)->type, strlen(type))) &&
        (!name || !strncmp(name, (dev + i)->name, strlen(name))))
      return dev + i;
#endif

  return NULL;
}

/**
 * Return OF prom pointer
 */
PUBLIC static inline
Address Boot_info::get_prom()
{
  return (Address)_prom_ptr;
}

PUBLIC static inline
void Boot_info::set_prom(Address prom_ptr)
{
  _prom_ptr = prom_ptr;
}

/**
 * Returns time base freuqency
 */
PUBLIC static inline
unsigned long Boot_info::get_time_base()
{
  return get_device(NULL, "cpu")->freq.time_freq;
}

IMPLEMENT static 
void Boot_info::init()
{
}


